#include "GraphTrack.h"

#include "GlCanvas.h"

GraphTrack::GraphTrack(TimeGraph* time_graph) {
    SetTimeGraph(time_graph);
}

void GraphTrack::Draw(GlCanvas* canvas, bool picking) {
  UNUSED(picking);

  TimeGraphLayout& layout = m_TimeGraph->GetLayout();
  float threadOffset = layout.GetThreadBlockStart(m_ID);
  //float trackHeight = GetHeight();
  float trackWidth = canvas->GetWorldWidth();

  SetPos(canvas->GetWorldTopLeftX(), threadOffset);
  SetSize(trackWidth, GetHeight());

  float x0 = m_Pos[0];
  float x1 = x0 + m_Size[0];

  float y0 = m_Pos[1];
  float y1 = y0 - m_Size[1];

  if (m_Picked) {
    glColor4ub(0, 128, 255, 128);
  }

  float track_z = layout.GetTrackZ();
  float text_z = layout.GetTextZ();

  glColor4ubv(&m_Color[0]);

  glBegin(GL_QUADS);
  glVertex3f(x0, y0, track_z);
  glVertex3f(x1, y0, track_z);
  glVertex3f(x1, y1, track_z);
  glVertex3f(x0, y1, track_z);
  glEnd();

  if (canvas->GetPickingManager().GetPicked() == this)
    glColor4ub(255, 255, 255, 255);

  glBegin(GL_LINES);
  glVertex3f(x0, y0, track_z);
  glVertex3f(x1, y0, track_z);
  glVertex3f(x1, y1, track_z);
  glVertex3f(x0, y1, track_z);
  glEnd();

  Color col(0, 128, 255, 128);
  Color colors[2];
  Fill(colors, col);

  // Current time window
  uint64_t min_ns = m_TimeGraph->GetTickFromUs(m_TimeGraph->GetMinTimeUs());
  uint64_t max_ns = m_TimeGraph->GetTickFromUs(m_TimeGraph->GetMaxTimeUs());
  double time_range = static_cast<float>(max_ns - min_ns);
  if (values_.size() < 2 || time_range == 0) return;

  auto it = values_.lower_bound(min_ns);
  if (it == values_.end()) return;
  uint64_t last_time = it->first;
  double last_normalized_value = (it->second - min_) * inv_value_range_;
  for (++it; it != values_.end(); ++it) {
    if (last_time > max_ns) break;
    uint64_t time = it->first;
    double normalized_value = (it->second - min_) * inv_value_range_;
    float base_y = m_Pos[1] - m_Size[1];
    float x0 = m_TimeGraph->GetWorldFromTick(last_time);
    float x1 = m_TimeGraph->GetWorldFromTick(time);
    float y0 = base_y + static_cast<float>(last_normalized_value) * m_Size[1];
    float y1 = base_y + static_cast<float>(normalized_value) * m_Size[1];
    Line line;
    line.m_Beg = Vec3(x0, y0, text_z);
    line.m_End = Vec3(x1, y1, text_z);
    m_TimeGraph->GetBatcher().AddLine(line, colors, PickingID::LINE, nullptr);

    last_time = time;
    last_normalized_value = normalized_value;
  }
}

void GraphTrack::OnDrag(int x, int y) {
  UNUSED(x);
  UNUSED(y);
}

void GraphTrack::AddTimer(const Timer& timer) {
  double value = *((double*)(&timer.m_UserData[0]));
  values_[timer.m_Start] = value;
  if (value > max_) max_ = value;
  if (value < min_) min_ = value;
  value_range_ = max_ - min_;

 if (value_range_ > 0) inv_value_range_ = 1.0 / value_range_;
}

//-----------------------------------------------------------------------------
float GraphTrack::GetHeight() const {
  TimeGraphLayout& layout = m_TimeGraph->GetLayout();
  float height = layout.GetTextBoxHeight() +
         layout.GetSpaceBetweenTracksAndThread() +
         layout.GetEventTrackHeight() + layout.GetTrackBottomMargin();
  return height;
}
