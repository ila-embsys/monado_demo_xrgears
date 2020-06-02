/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(binding = 0) uniform UBOMatrices
{
  mat4 normal;
  mat4 model;
}
uboModel;

layout(binding = 2) uniform UBOCamera
{
  mat4 projection;
  mat4 view;
}
uboCamera;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outWorldPos;

out gl_PerVertex
{
  vec4 gl_Position;
};

void
main()
{
  outNormal = mat3(uboModel.normal) * inNormal;
  outWorldPos = uboModel.model * vec4(inPos.xyz, 1.0);
  gl_Position = uboCamera.projection * uboCamera.view * outWorldPos;
}
