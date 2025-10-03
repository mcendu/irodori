/*
 * Copyright (c) 2025 McEndu.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#version 460 core

layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_uv;

layout(location = 2) in mat3 i_colorGradientMatrix;
layout(location = 5) in mat3 i_alphaGradientMatrix;
layout(location = 8) in float i_timeScale;
layout(location = 9) in float i_zOffset;
layout(location = 10) in vec2 i_uvOffset;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out float v_time;
layout(location = 2) out mat3 v_colorGradientMatrix;
layout(location = 5) out mat3 v_alphaGradientMatrix;

layout(set = 1, std140, binding = 0) uniform uniforms {
    mat4 projection;
    mat4 transform;
    float time;
};

void main() {
  vec4 pos = a_pos + vec4(0.0f, 0.0f, i_zOffset, 0.0f);
  gl_Position = projection * transform * pos;
  v_uv = a_uv + i_uvOffset;
  v_time = clamp(time * i_timeScale, 0.0f, 127.0f / 128.0f);
  v_colorGradientMatrix = i_colorGradientMatrix;
  v_alphaGradientMatrix = i_alphaGradientMatrix;
}
