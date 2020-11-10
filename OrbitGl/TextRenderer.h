// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <freetype-gl/mat4.h>

#include <map>

#include "Batcher.h"
#include "OpenGl.h"
#include "TextBox.h"

namespace ftgl {
struct vertex_buffer_t;
struct texture_font_t;
}  // namespace ftgl

class TextRenderer {
 public:
  explicit TextRenderer(uint32_t font_size);
  ~TextRenderer();

  void Init();
  void Display(Batcher* batcher);
  void DisplayLayer(Batcher* batcher, float layer);
  void AddText(const char* a_Text, float a_X, float a_Y, float a_Z, const Color& a_Color,
               uint32_t font_size, float a_MaxSize = -1.f, bool a_RightJustified = false,
               Vec2* out_text_pos = nullptr, Vec2* out_text_size = nullptr);
  // Returns the width of the rendered string.
  int AddTextTrailingCharsPrioritized(const char* a_Text, float a_X, float a_Y, float a_Z,
                                      const Color& a_Color, size_t a_TrailingCharsLength,
                                      uint32_t font_size, float a_MaxSize);
  int AddText2D(const char* a_Text, int a_X, int a_Y, float a_Z, const Color& a_Color,
                float a_MaxSize = -1.f, bool a_RightJustified = false, bool a_InvertY = true);

  int GetStringWidth(const char* a_Text) const;
  [[nodiscard]] std::vector<float> GetLayers() const;
  void Clear();
  void SetCanvas(class GlCanvas* a_Canvas) { m_Canvas = a_Canvas; }
  const GlCanvas* GetCanvas() const { return m_Canvas; }
  GlCanvas* GetCanvas() { return m_Canvas; }
  void ToggleDrawOutline() { m_DrawOutline = !m_DrawOutline; }
  void SetFontSize(uint32_t a_Size);
  uint32_t GetFontSize() const;

 protected:
  void AddTextInternal(texture_font_t* font, const char* text, const vec4& color, vec2* pen,
                       float a_MaxSize = -1.f, float a_Z = -0.01f, vec2* out_text_pos = nullptr,
                       vec2* out_text_size = nullptr);
  void ToScreenSpace(float a_X, float a_Y, float& o_X, float& o_Y);
  float ToScreenSpace(float a_Size);
  void DrawOutline(Batcher* batcher, vertex_buffer_t* a_Buffer);

 private:
  texture_atlas_t* m_Atlas;
  std::unordered_map<float, vertex_buffer_t*> buffers_by_layer_;
  texture_font_t* m_Font;
  std::map<int, texture_font_t*> m_FontsBySize;
  uint32_t current_font_size_;
  GlCanvas* m_Canvas;
  GLuint m_Shader;
  mat4 m_Model;
  mat4 m_View;
  mat4 m_Proj;
  vec2 m_Pen;
  bool m_Initialized;
  bool m_DrawOutline;
};

inline vec4 ColorToVec4(const Color& a_Col) {
  const float coeff = 1.f / 255.f;
  vec4 color;
  color.r = a_Col[0] * coeff;
  color.g = a_Col[1] * coeff;
  color.b = a_Col[2] * coeff;
  color.a = a_Col[3] * coeff;
  return color;
}
