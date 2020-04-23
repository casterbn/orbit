#ifndef ORBIT_LINUX_TRACING_UPROBES_FUNCTION_CALL_MANAGER_H_
#define ORBIT_LINUX_TRACING_UPROBES_FUNCTION_CALL_MANAGER_H_

#include <OrbitBase/Logging.h>
#include <OrbitLinuxTracing/Events.h>

#include <stack>

#include "absl/container/flat_hash_map.h"

namespace LinuxTracing {

// Keeps a stack, for every thread, of the open uprobes and matches them with
// the uretprobes to produce FunctionCall objects.
class UprobesFunctionCallManager {
 public:
  UprobesFunctionCallManager() = default;

  UprobesFunctionCallManager(const UprobesFunctionCallManager&) = delete;
  UprobesFunctionCallManager& operator=(const UprobesFunctionCallManager&) =
      delete;

  UprobesFunctionCallManager(UprobesFunctionCallManager&&) = default;
  UprobesFunctionCallManager& operator=(UprobesFunctionCallManager&&) = default;

  void ProcessUprobes(pid_t tid, uint64_t function_address,
                      uint64_t begin_timestamp) {
    auto& tid_uprobes_stack = tid_uprobes_stacks_[tid];
    tid_uprobes_stack.emplace(function_address, begin_timestamp);
  }

  std::optional<FunctionCall> ProcessUretprobes(pid_t tid,
                                                uint64_t end_timestamp,
                                                uint64_t return_value) {
    if (!tid_uprobes_stacks_.contains(tid)) {
      return std::optional<FunctionCall>{};
    }

    auto& tid_uprobes_stack = tid_uprobes_stacks_.at(tid);

    // As we erase the stack for this thread as soon as it becomes empty.
    CHECK(!tid_uprobes_stack.empty());

    auto function_call = std::make_optional<FunctionCall>(
        tid, tid_uprobes_stack.top().function_address,
        tid_uprobes_stack.top().begin_timestamp, end_timestamp,
        tid_uprobes_stack.size() - 1, return_value);
    tid_uprobes_stack.pop();
    if (tid_uprobes_stack.empty()) {
      tid_uprobes_stacks_.erase(tid);
    }
    return function_call;
  }

 private:
  struct OpenUprobes {
    OpenUprobes(uint64_t function_address, uint64_t begin_timestamp)
        : function_address{function_address},
          begin_timestamp{begin_timestamp} {}
    uint64_t function_address;
    uint64_t begin_timestamp;
  };

  // This map keeps the stack of the dynamically-instrumented functions entered.
  absl::flat_hash_map<pid_t, std::stack<OpenUprobes, std::vector<OpenUprobes>>>
      tid_uprobes_stacks_{};
};

}  // namespace LinuxTracing

#endif  // ORBIT_LINUX_TRACING_UPROBES_FUNCTION_CALL_MANAGER_H_
