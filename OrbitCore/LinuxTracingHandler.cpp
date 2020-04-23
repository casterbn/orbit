#include "LinuxTracingHandler.h"

#include <functional>

#include "Callstack.h"
#include "ContextSwitch.h"
#include "OrbitModule.h"
#include "Params.h"
#include "TcpServer.h"
#include "absl/flags/flag.h"
#include "llvm/Demangle/Demangle.h"

// TODO: Remove this flag once we enable specifying the sampling frequency or
//  period in the client.
ABSL_FLAG(uint16_t, sampling_frequency, 1000,
          "Frequency of callstack sampling in samples per second");

// TODO: This is a temporary feature flag. Remove this once we enable this
//  globally.
ABSL_FLAG(bool, trace_gpu_driver, false,
          "Enables tracing of GPU driver tracepoint events");

void LinuxTracingHandler::Start() {
  pid_t pid = target_process_->GetID();

  double sampling_frequency = absl::GetFlag(FLAGS_sampling_frequency);

  std::vector<LinuxTracing::Function> selected_functions;
  selected_functions.reserve(selected_function_map_->size());
  for (const auto& pair : *selected_function_map_) {
    const auto& function = pair.second;
    selected_functions.emplace_back(function->GetLoadedModuleName(),
                                    function->Offset(),
                                    function->GetVirtualAddress());
  }

  tracer_ = std::make_unique<LinuxTracing::Tracer>(pid, sampling_frequency,
                                                   selected_functions);

  tracer_->SetListener(this);

  tracer_->SetTraceContextSwitches(GParams.m_TrackContextSwitches);
  tracer_->SetTraceCallstacks(true);
  tracer_->SetTraceInstrumentedFunctions(true);
  tracer_->SetTraceGpuDriver(absl::GetFlag(FLAGS_trace_gpu_driver));

  tracer_->Start();
}

void LinuxTracingHandler::Stop() {
  tracer_->Stop();
  tracer_.reset();
}

void LinuxTracingHandler::OnTid(pid_t tid) {
  // TODO: This doesn't seem to be of any use at the moment: it has the effect
  //  of adding the tid to Process::m_ThreadIds in OrbitProcess.h, but that
  //  field is never actually used. Investigate whether m_ThreadIds should be
  //  used or if this call should be removed.
  target_process_->AddThreadId(tid);
}

void LinuxTracingHandler::ProcessCallstackEvent(LinuxCallstackEvent&& event) {
  CallStack cs = event.m_CS;
  if (sampling_profiler_->HasCallStack(cs.Hash())) {
    CallstackEvent hashed_callstack;
    hashed_callstack.m_Id = cs.m_Hash;
    hashed_callstack.m_TID = cs.m_ThreadId;
    hashed_callstack.m_Time = event.m_time;

    session_->RecordHashedCallstack(std::move(hashed_callstack));
  } else {
    session_->RecordCallstack(std::move(event));
  }

  // TODO: Is this needed for the case when the call stack already cached?
  sampling_profiler_->AddCallStack(cs);
}

void LinuxTracingHandler::OnContextSwitchIn(
    const LinuxTracing::ContextSwitchIn& context_switch_in) {
  ++(*num_context_switches_);

  ContextSwitch context_switch(ContextSwitch::In);
  context_switch.m_ProcessId = context_switch_in.GetPid();
  context_switch.m_ThreadId = context_switch_in.GetTid();
  context_switch.m_Time = context_switch_in.GetTimestampNs();
  context_switch.m_ProcessorIndex = context_switch_in.GetCore();
  context_switch.m_ProcessorNumber = context_switch_in.GetCore();

  session_->RecordContextSwitch(std::move(context_switch));
}

void LinuxTracingHandler::OnContextSwitchOut(
    const LinuxTracing::ContextSwitchOut& context_switch_out) {
  ++(*num_context_switches_);

  ContextSwitch context_switch(ContextSwitch::Out);
  context_switch.m_ProcessId = context_switch_out.GetPid();
  context_switch.m_ThreadId = context_switch_out.GetTid();
  context_switch.m_Time = context_switch_out.GetTimestampNs();
  context_switch.m_ProcessorIndex = context_switch_out.GetCore();
  context_switch.m_ProcessorNumber = context_switch_out.GetCore();

  session_->RecordContextSwitch(std::move(context_switch));
}

void LinuxTracingHandler::OnCallstack(
    const LinuxTracing::Callstack& callstack) {
  CallStack cs;
  cs.m_ThreadId = callstack.GetTid();

  for (const auto& frame : callstack.GetFrames()) {
    uint64_t address = frame.GetPc();

    if (!frame.GetFunctionName().empty() &&
        !target_process_->HasAddressInfo(address)) {
      LinuxAddressInfo address_info;
      address_info.address = address;
      address_info.module_name = frame.GetMapName();
      address_info.function_name = llvm::demangle(frame.GetFunctionName());
      address_info.offset_in_function = frame.GetFunctionOffset();

      // TODO: Move this out of here...
      std::string message_data = SerializeObjectBinary(address_info);
      GTcpServer->Send(Msg_RemoteLinuxAddressInfo, message_data.c_str(),
                       message_data.size());

      target_process_->AddAddressInfo(std::move(address_info));
    }

    cs.m_Data.push_back(address);
  }

  cs.m_Depth = cs.m_Data.size();

  ProcessCallstackEvent({"", callstack.GetTimestampNs(), 1, cs});
}

void LinuxTracingHandler::OnFunctionCall(
    const LinuxTracing::FunctionCall& function_call) {
  Timer timer;
  timer.m_TID = function_call.GetTid();
  timer.m_Start = function_call.GetBeginTimestampNs();
  timer.m_End = function_call.GetEndTimestampNs();
  timer.m_Depth = static_cast<uint8_t>(function_call.GetDepth());
  timer.m_FunctionAddress = function_call.GetVirtualAddress();
  timer.m_UserData[0] = function_call.GetIntegerReturnValue();

  session_->RecordTimer(std::move(timer));
}

pid_t LinuxTracingHandler::TimelineToThreadId(std::string_view timeline) {
  auto it = timeline_to_thread_id_.find(timeline);
  if (it != timeline_to_thread_id_.end()) {
    return it->second;
  }
  pid_t new_id = current_timeline_thread_id_;
  current_timeline_thread_id_++;

  timeline_to_thread_id_.emplace(timeline, new_id);
  return new_id;
}

void LinuxTracingHandler::OnGpuJob(const LinuxTracing::GpuJob& gpu_job) {
  Timer timer_user_to_sched;
  timer_user_to_sched.m_TID = TimelineToThreadId(gpu_job.GetTimeline());
  timer_user_to_sched.m_SubmitTID = gpu_job.GetTid();
  timer_user_to_sched.m_Start = gpu_job.GetAmdgpuCsIoctlTimeNs();
  timer_user_to_sched.m_End = gpu_job.GetAmdgpuSchedRunJobTimeNs();
  timer_user_to_sched.m_Depth = gpu_job.GetDepth();

  constexpr const char* sw_queue = "sw queue";
  uint64_t hash = StringHash(sw_queue);
  session_->SendKeyAndString(hash, sw_queue);
  timer_user_to_sched.m_UserData[0] = hash;

  uint64_t timeline_hash = StringHash(gpu_job.GetTimeline());
  session_->SendKeyAndString(timeline_hash, gpu_job.GetTimeline());
  timer_user_to_sched.m_UserData[1] = timeline_hash;

  timer_user_to_sched.m_Type = Timer::GPU_ACTIVITY;
  session_->RecordTimer(std::move(timer_user_to_sched));

  Timer timer_sched_to_start;
  timer_sched_to_start.m_TID = TimelineToThreadId(gpu_job.GetTimeline());
  timer_sched_to_start.m_SubmitTID = gpu_job.GetTid();
  timer_sched_to_start.m_Start = gpu_job.GetAmdgpuSchedRunJobTimeNs();
  timer_sched_to_start.m_End = gpu_job.GetGpuHardwareStartTimeNs();
  timer_sched_to_start.m_Depth = gpu_job.GetDepth();

  constexpr const char* hw_queue = "hw queue";
  hash = StringHash(hw_queue);
  session_->SendKeyAndString(hash, hw_queue);

  timer_sched_to_start.m_UserData[0] = hash;
  timer_sched_to_start.m_UserData[1] = timeline_hash;

  timer_sched_to_start.m_Type = Timer::GPU_ACTIVITY;
  session_->RecordTimer(std::move(timer_sched_to_start));

  Timer timer_start_to_finish;
  timer_start_to_finish.m_TID = TimelineToThreadId(gpu_job.GetTimeline());
  timer_start_to_finish.m_SubmitTID = gpu_job.GetTid();
  timer_start_to_finish.m_Start = gpu_job.GetGpuHardwareStartTimeNs();
  timer_start_to_finish.m_End = gpu_job.GetDmaFenceSignaledTimeNs();
  timer_start_to_finish.m_Depth = gpu_job.GetDepth();

  constexpr const char* hw_execution = "hw execution";
  hash = StringHash(hw_execution);
  session_->SendKeyAndString(hash, hw_execution);
  timer_start_to_finish.m_UserData[0] = hash;
  timer_start_to_finish.m_UserData[1] = timeline_hash;

  timer_start_to_finish.m_Type = Timer::GPU_ACTIVITY;
  session_->RecordTimer(std::move(timer_start_to_finish));
}
