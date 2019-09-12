/*
 * xrgears
 *
 * Copyright 2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 450

layout(location = 0) out vec2 out_uv;

out gl_PerVertex
{
  vec4 gl_Position;
};

void
main()
{
  out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}
