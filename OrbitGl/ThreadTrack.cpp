#include "ThreadTrack.h"

#include <limits>

#include "Capture.h"
#include "EventTrack.h"
#include "GlCanvas.h"
#include "OrbitUnreal.h"
#include "Systrace.h"
#include "TimeGraph.h"

#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"

// TODO: Remove this flag once we have a way to toggle the display return values
ABSL_FLAG(bool, show_return_values, false, "Show return values on time slices");

//-----------------------------------------------------------------------------
ThreadTrack::ThreadTrack(TimeGraph* a_TimeGraph, uint32_t a_ThreadID) {
  m_TimeGraph = a_TimeGraph;
  m_ID = a_ThreadID;
  m_TextRenderer = a_TimeGraph->GetTextRenderer();
  m_ThreadID = a_ThreadID;

  m_NumTimers = 0;
  m_MinTime = std::numeric_limits<TickType>::max();
  m_MaxTime = std::numeric_limits<TickType>::min();

  m_EventTrack = std::make_shared<EventTrack>(a_TimeGraph);
  m_EventTrack->SetThreadId(a_ThreadID);
}

//-----------------------------------------------------------------------------
void ThreadTrack::Draw(GlCanvas* a_Canvas, bool a_Picking) {
  // Scheduling information is held in thread "0", don't draw as threadtrack.
  // TODO: Make a proper "SchedTrack" instead of hack.
  if (m_ID == 0) return;

  TimeGraphLayout& layout = m_TimeGraph->GetLayout();
  float threadOffset = layout.GetThreadBlockStart(m_ID);
  float trackHeight = GetHeight();
  float trackWidth = a_Canvas->GetWorldWidth();

  SetPos(a_Canvas->GetWorldTopLeftX(), threadOffset);
  SetSize(trackWidth, trackHeight);

  Track::Draw(a_Canvas, a_Picking);

  // Event track
  m_EventTrack->SetPos(m_Pos[0], m_Pos[1]);
  m_EventTrack->SetSize(a_Canvas->GetWorldWidth(),
                        m_TimeGraph->GetLayout().GetEventTrackHeight());
  m_EventTrack->Draw(a_Canvas, a_Picking);
}

//-----------------------------------------------------------------------------
std::string GetExtraInfo(const Timer& a_Timer) {
  std::string info;
  static bool show_return_value = absl::GetFlag(FLAGS_show_return_values);
  if (!Capture::IsCapturing() && a_Timer.GetType() == Timer::UNREAL_OBJECT) {
    info =
        "[" + ws2s(GOrbitUnreal.GetObjectNames()[a_Timer.m_UserData[0]]) + "]";
  } else if (show_return_value && (a_Timer.GetType() == Timer::NONE)) {
    info = absl::StrFormat("[%lu]", a_Timer.m_UserData[0]);
  }
  return info;
}

//-----------------------------------------------------------------------------
void ThreadTrack::UpdatePrimitives(TimeGraph* time_graph, Batcher* batcher,
                                   TextRenderer* text_renderer,
                                   GlCanvas* canvas, double min_us,
                                   double max_us, TickType min_tick) {

  const TimeGraphLayout& m_Layout = time_graph->GetLayout();
  const TextBox& m_SceneBox = canvas->GetSceneBox();
  float minX = m_SceneBox.GetPosX();

  double m_TimeWindowUs = max_us - min_us;
  float m_WorldStartX = canvas->GetWorldTopLeftX();
  float m_WorldWidth = canvas->GetWorldWidth();
  double invTimeWindow = 1.0 / m_TimeWindowUs;
  TickType rawStart = time_graph->GetTickFromUs(min_us);
  TickType rawStop = time_graph->GetTickFromUs(max_us);

  std::vector<std::shared_ptr<TimerChain>> depthChain = GetTimers();
  for (auto& textBoxes : depthChain) {
    if (textBoxes == nullptr) break;

    for (TextBox& textBox : *textBoxes) {
      const Timer& timer = textBox.GetTimer();

      if (!(rawStart > timer.m_End || rawStop < timer.m_Start)) {
        double start = MicroSecondsFromTicks(min_tick, timer.m_Start) - min_us;
        double end = MicroSecondsFromTicks(min_tick, timer.m_End) - min_us;
        double elapsed = end - start;

        double NormalizedStart = start * invTimeWindow;
        double NormalizedLength = elapsed * invTimeWindow;

        bool isCore = timer.IsType(Timer::CORE_ACTIVITY);

        float threadOffset = 0.f;
            // !isCore ? m_Layout.GetThreadOffset(timer.m_TID, timer.m_Depth)
            //         : m_Layout.GetCoreOffset(timer.m_Processor);

        float boxHeight = !isCore ? m_Layout.GetTextBoxHeight()
                                  : m_Layout.GetTextCoresHeight();

        float WorldTimerStartX =
            float(m_WorldStartX + NormalizedStart * m_WorldWidth);
        float WorldTimerWidth = float(NormalizedLength * m_WorldWidth);

        Vec2 pos(WorldTimerStartX, threadOffset);
        Vec2 size(WorldTimerWidth, boxHeight);

        textBox.SetPos(pos);
        textBox.SetSize(size);

        if (!isCore) {
          UpdateDepth(timer.m_Depth + 1);
        }

        bool isContextSwitch = timer.IsType(Timer::THREAD_ACTIVITY);
        bool isVisibleWidth = NormalizedLength * canvas->getWidth() > 1;
        bool isSameProcessIdAsTarget =
            isCore && Capture::GTargetProcess != nullptr
                ? timer.m_PID == Capture::GTargetProcess->GetID()
                : true;
        bool isSameThreadIdAsSelected =
            isCore && (timer.m_TID == Capture::GSelectedThreadId);
        bool isInactive =
            (!isContextSwitch && timer.m_FunctionAddress &&
             (!Capture::GVisibleFunctionsMap.empty() &&
              Capture::GVisibleFunctionsMap[timer.m_FunctionAddress] ==
                  nullptr)) ||
            (Capture::GSelectedThreadId != 0 && isCore &&
             !isSameThreadIdAsSelected);
        bool isSelected = &textBox == Capture::GSelectedTextBox;

        const unsigned char g = 100;
        Color grey(g, g, g, 255);
        static Color selectionColor(0, 128, 255, 255);

        Color col = time_graph->GetTimesliceColor(timer);

        if (isSelected) {
          col = selectionColor;
        } else if (!isSameThreadIdAsSelected &&
                   (isInactive || !isSameProcessIdAsTarget)) {
          col = grey;
        }

        textBox.SetColor(col[0], col[1], col[2]);
        static int oddAlpha = 210;
        if (!(timer.m_Depth & 0x1)) {
          col[3] = oddAlpha;
        }

        float z = isInactive ? GlCanvas::Z_VALUE_BOX_INACTIVE
                             : GlCanvas::Z_VALUE_BOX_ACTIVE;

        if (isVisibleWidth) {
          Box box;
          box.m_Vertices[0] = Vec3(pos[0], pos[1], z);
          box.m_Vertices[1] = Vec3(pos[0], pos[1] + size[1], z);
          box.m_Vertices[2] = Vec3(pos[0] + size[0], pos[1] + size[1], z);
          box.m_Vertices[3] = Vec3(pos[0] + size[0], pos[1], z);
          Color colors[4];
          Fill(colors, col);

          static float coeff = 0.94f;
          Vec3 dark = Vec3(col[0], col[1], col[2]) * coeff;
          colors[1] = Color((unsigned char)dark[0], (unsigned char)dark[1],
                            (unsigned char)dark[2], (unsigned char)col[3]);
          colors[0] = colors[1];
          batcher->AddBox(box, colors, PickingID::BOX, &textBox);

          if (!isContextSwitch && textBox.GetText().empty()) {
            double elapsedMillis = ((double)elapsed) * 0.001;
            std::string time = GetPrettyTime(elapsedMillis);
            Function* func =
                Capture::GSelectedFunctionsMap[timer.m_FunctionAddress];

            textBox.SetElapsedTimeTextLength(time.length());

            const char* name = nullptr;
            if (func) {
              std::string extraInfo = GetExtraInfo(timer);
              name = func->PrettyName().c_str();
              std::string text = absl::StrFormat(
                  "%s %s %s", name, extraInfo.c_str(), time.c_str());

              textBox.SetText(text);
            } else if (timer.m_Type == Timer::INTROSPECTION) {
              std::string text = absl::StrFormat("%s %s",
                                                 time_graph->GetStringManager()
                                                     ->Get(timer.m_UserData[0])
                                                     .value_or(""),
                                                 time.c_str());
              textBox.SetText(text);
            } else if (timer.m_Type == Timer::GPU_ACTIVITY) {
              std::string text = absl::StrFormat(
                  "%s; submitter: %d  %s",
                  time_graph->GetStringManager()->Get(timer.m_UserData[0]).value_or(""),
                  timer.m_SubmitTID, time.c_str());
              textBox.SetText(text);
            } else if (!SystraceManager::Get().IsEmpty()) {
              textBox.SetText(SystraceManager::Get().GetFunctionName(
                  timer.m_FunctionAddress));
            } else if (!Capture::IsCapturing()) {
              // GZoneNames is populated when capturing, prevent race
              // by accessing it only when not capturing.
              auto it = Capture::GZoneNames.find(timer.m_FunctionAddress);
              if (it != Capture::GZoneNames.end()) {
                name = it->second.c_str();
                std::string text = absl::StrFormat("%s %s", name, time.c_str());
                textBox.SetText(text);
              }
            }
          }

          if (!isCore) {
            static Color s_Color(255, 255, 255, 255);

            const Vec2& boxPos = textBox.GetPos();
            const Vec2& boxSize = textBox.GetSize();
            float posX = std::max(boxPos[0], minX);
            float maxSize = boxPos[0] + boxSize[0] - posX;
            text_renderer->AddTextTrailingCharsPrioritized(
                textBox.GetText().c_str(), posX, textBox.GetPosY() + 1.f,
                GlCanvas::Z_VALUE_TEXT, s_Color,
                textBox.GetElapsedTimeTextLength(), maxSize);
          }
        } else {
          Line line;
          line.m_Beg = Vec3(pos[0], pos[1], z);
          line.m_End = Vec3(pos[0], pos[1] + size[1], z);
          Color colors[2];
          Fill(colors, col);
          batcher->AddLine(line, colors, PickingID::LINE, &textBox);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ThreadTrack::OnDrag(int a_X, int a_Y) { Track::OnDrag(a_X, a_Y); }

//-----------------------------------------------------------------------------
void ThreadTrack::OnTimer(const Timer& a_Timer) {
  UpdateDepth(a_Timer.m_Depth + 1);
  TextBox textBox(Vec2(0, 0), Vec2(0, 0), "", Color(255, 0, 0, 255));
  textBox.SetTimer(a_Timer);

  std::shared_ptr<TimerChain> timerChain = m_Timers[a_Timer.m_Depth];
  if (timerChain == nullptr) {
    timerChain = std::make_shared<TimerChain>();
    m_Timers[a_Timer.m_Depth] = timerChain;
  }
  timerChain->push_back(textBox);
  ++m_NumTimers;
  if (a_Timer.m_Start < m_MinTime) m_MinTime = a_Timer.m_Start;
  if (a_Timer.m_End > m_MaxTime) m_MaxTime = a_Timer.m_End;
}

//-----------------------------------------------------------------------------
float ThreadTrack::GetHeight() const {
  TimeGraphLayout& layout = m_TimeGraph->GetLayout();
  return layout.GetTextBoxHeight() * GetDepth() +
         layout.GetSpaceBetweenTracksAndThread() +
         layout.GetEventTrackHeight() + layout.GetTrackBottomMargin();
}

//-----------------------------------------------------------------------------
std::vector<std::shared_ptr<TimerChain>> ThreadTrack::GetTimers() {
  std::vector<std::shared_ptr<TimerChain>> timers;
  ScopeLock lock(m_Mutex);
  for (auto& timerChain : m_Timers) {
    timers.push_back(timerChain.second);
  }
  return timers;
}

//-----------------------------------------------------------------------------
const TextBox* ThreadTrack::GetFirstAfterTime(TickType a_Tick,
                                              uint32_t a_Depth) const {
  std::shared_ptr<TimerChain> textBoxes = GetTimers(a_Depth);
  if (textBoxes == nullptr) return nullptr;

  // TODO: do better than linear search...
  for (TextBox& textBox : *textBoxes) {
    if (textBox.GetTimer().m_Start > a_Tick) {
      return &textBox;
    }
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
const TextBox* ThreadTrack::GetFirstBeforeTime(TickType a_Tick,
                                               uint32_t a_Depth) const {
  std::shared_ptr<TimerChain> textBoxes = GetTimers(a_Depth);
  if (textBoxes == nullptr) return nullptr;

  TextBox* textBox = nullptr;

  // TODO: do better than linear search...
  for (TextBox& box : *textBoxes) {
    if (box.GetTimer().m_Start > a_Tick) {
      return textBox;
    }

    textBox = &box;
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
std::shared_ptr<TimerChain> ThreadTrack::GetTimers(uint32_t a_Depth) const {
  ScopeLock lock(m_Mutex);
  auto it = m_Timers.find(a_Depth);
  if (it != m_Timers.end()) return it->second;
  return nullptr;
}

//-----------------------------------------------------------------------------
const TextBox* ThreadTrack::GetLeft(TextBox* a_TextBox) const {
  const Timer& timer = a_TextBox->GetTimer();
  if (timer.m_TID == m_ThreadID) {
    std::shared_ptr<TimerChain> timers = GetTimers(timer.m_Depth);
    if (timers) return timers->GetElementBefore(a_TextBox);
  }
  return nullptr;
}

//-----------------------------------------------------------------------------
const TextBox* ThreadTrack::GetRight(TextBox* a_TextBox) const {
  const Timer& timer = a_TextBox->GetTimer();
  if (timer.m_TID == m_ThreadID) {
    std::shared_ptr<TimerChain> timers = GetTimers(timer.m_Depth);
    if (timers) return timers->GetElementAfter(a_TextBox);
  }
  return nullptr;
}

//-----------------------------------------------------------------------------
const TextBox* ThreadTrack::GetUp(TextBox* a_TextBox) const {
  const Timer& timer = a_TextBox->GetTimer();
  return GetFirstBeforeTime(timer.m_Start, timer.m_Depth - 1);
}

//-----------------------------------------------------------------------------
const TextBox* ThreadTrack::GetDown(TextBox* a_TextBox) const {
  const Timer& timer = a_TextBox->GetTimer();
  return GetFirstAfterTime(timer.m_Start, timer.m_Depth + 1);
}

//-----------------------------------------------------------------------------
std::vector<std::shared_ptr<TimerChain>> ThreadTrack::GetAllChains() {
  std::vector<std::shared_ptr<TimerChain>> chains;
  for (const auto& pair : m_Timers) {
    chains.push_back(pair.second);
  }
  return chains;
}

//-----------------------------------------------------------------------------
void ThreadTrack::SetEventTrackColor(Color color) {
  ScopeLock lock(m_Mutex);
  m_EventTrack->SetColor(color);
}
