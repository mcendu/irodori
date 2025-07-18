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

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color;

layout(set = 2, binding = 0) uniform sampler2D tex;

layout(set = 3, std140, binding = 0) uniform gradientSpec {
    mat3 colorGradientMatrix;
    mat3 alphaGradientMatrix;
    float time;
};

void main() {
  vec4 base = texture(tex, uv);
  float baseAlpha = base.r;

  vec3 basePos = { base.g, time, 1.0 };
  vec3 colorPos = colorGradientMatrix * basePos;
  vec3 alphaPos = alphaGradientMatrix * basePos;

  float alpha = baseAlpha * texture(tex, alphaPos.xy / alphaPos.z).r;
  color = vec4(texture(tex, colorPos.xy / colorPos.z).rgb, alpha);
}
