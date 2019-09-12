/*
 * xrgears
 *
 * Copyright 2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 450

layout(set = 0, binding = 0) uniform UBO
{
  mat4 vp;
}
ubo;
layout(set = 0, binding = 1) uniform sampler2D map;

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

const float PI = 3.1416;

void
main()
{
  vec2 frag_coord = vec2(in_uv) * 2 - 1;
  vec4 view_dir = normalize(ubo.vp * vec4(frag_coord, 1, 1));

  float u = atan(view_dir.x, -view_dir.z) / (2 * PI) + 0.5;
  float v = acos(-view_dir.y) / PI;

  out_color = texture(map, vec2(u, v));
}
