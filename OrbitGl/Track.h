//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "Batcher.h"
#include "BlockChain.h"
#include "CoreMath.h"
#include "PickingManager.h"
#include "Profiling.h"
#include "TextBox.h"
#include "TextRenderer.h"
#include "TimeGraphLayout.h"

class GlCanvas;
class TimeGraph;

typedef BlockChain<TextBox, 4 * 1024> TimerChain;

//-----------------------------------------------------------------------------
class Track : public Pickable {
 public:

  enum Type {
    kThreadTrack,
    kGraphTrack,
    kGpuTrack,
    kSchedTrack,
    kUnknown,
  };

  Track();
  ~Track() override = default;

  // Pickable
  void Draw(GlCanvas* a_Canvas, bool a_Picking) override;
  virtual void UpdatePrimitives(TimeGraph* time_graph, Batcher* batcher,
                                TextRenderer* text_renderer, GlCanvas* canvas,
                                double min_us, double max_us,
                                TickType min_tick);
  void OnPick(int a_X, int a_Y) override;
  void OnRelease() override;
  void OnDrag(int a_X, int a_Y) override;
  bool Draggable() override { return true; }
  bool Movable() override { return true; }

  virtual Type GetType() const = 0;
  virtual void AddTimer(const Timer&) {}

  virtual float GetHeight() const { return 0.f; };
  bool GetVisible() const { return m_Visible; }
  void SetVisible(bool value) { m_Visible = value; }

  uint32_t GetNumTimers() const { return m_NumTimers; }
  TickType GetMinTime() const { return m_MinTime; }
  TickType GetMaxTime() const { return m_MaxTime; }

  virtual std::vector<std::shared_ptr<TimerChain>> GetTimers() { return {}; }
  virtual std::vector<std::shared_ptr<TimerChain>> GetAllChains() { return {}; }

  bool IsMoving() const { return m_Moving; }
  Vec2 GetMoveDelta() const {
    return m_Moving ? m_MousePos[1] - m_MousePos[0] : Vec2(0, 0);
  }
  void SetName(const std::string& a_Name) { m_Name = a_Name; }

  enum LabelDisplayMode { NAME_AND_TID, TID_ONLY, NAME_ONLY, EMPTY };

  void SetLabelDisplayMode(LabelDisplayMode label_display_mode) {
    label_display_mode_ = label_display_mode;
  }

  const std::string& GetName() const { return m_Name; }
  void SetTimeGraph(TimeGraph* a_TimeGraph) { m_TimeGraph = a_TimeGraph; }
  void SetPos(float a_X, float a_Y);
  Vec2 GetPos() const { return m_Pos; }
  void SetSize(float a_SizeX, float a_SizeY);
  void SetID(uint32_t a_ID) { m_ID = a_ID; }
  uint32_t GetID() const { return m_ID; }
  void SetColor(Color a_Color) { m_Color = a_Color; }

 protected:
  GlCanvas* m_Canvas;
  TimeGraph* m_TimeGraph;
  Vec2 m_Pos;
  Vec2 m_Size;
  Vec2 m_MousePos[2];
  Vec2 m_PickingOffset;
  bool m_Picked;
  bool m_Moving;
  std::string m_Name;
  uint32_t m_ID;
  LabelDisplayMode label_display_mode_;
  Color m_Color;
  bool m_Visible = true;
  std::atomic<uint32_t> m_NumTimers;
  std::atomic<TickType> m_MinTime;
  std::atomic<TickType> m_MaxTime;
  bool m_PickingEnabled = false;
  Type type_ = kUnknown;
};
