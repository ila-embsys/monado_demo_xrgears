/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <vector>
#include "glm_inc.hpp"
#include "vulkan_buffer.h"
#include "vulkan_device.h"
#include "log.h"

struct Material
{
  // Parameter block used as push constant block
  struct PushBlock
  {
    float roughness;
    float metallic;
    float r, g, b;
  } params;
  std::string name;
  Material() {}
  Material(std::string n, glm::vec3 c, float r, float m) : name(n)
  {
    params.roughness = r;
    params.metallic = m;
    params.r = c.r;
    params.g = c.g;
    params.b = c.b;
  }
};

struct Vertex
{
  float pos[3];
  float normal[3];

  Vertex(const glm::vec3& p, const glm::vec3& n)
  {
    pos[0] = p.x;
    pos[1] = p.y;
    pos[2] = p.z;
    normal[0] = n.x;
    normal[1] = n.y;
    normal[2] = n.z;
  }
};

struct GearInfo
{
  float inner_radius;
  float outer_radius;
  float width;
  int tooth_count;
  float tooth_depth;
};


class Gear
{
public:
  struct UBO
  {
    glm::mat4 normal;
    glm::mat4 model;
  } ubo;

  VkDescriptorSet descriptor_sets[2];

  struct NodeInfo
  {
    glm::vec3 position;
    float rotation_speed;
    float rotation_offset;
    Material material;
  } info;

  vulkan_buffer uniformBuffer;
  vulkan_buffer vertexBuffer;
  vulkan_buffer indexBuffer;
  uint32_t indexCount;

  Gear() {}

  virtual ~Gear()
  {
    vulkan_buffer_destroy(&uniformBuffer);
    vulkan_buffer_destroy(&vertexBuffer);
    vulkan_buffer_destroy(&indexBuffer);
  }

  void
  setMateral(const Material& m)
  {
    info.material = m;
  }

  void
  setPosition(const glm::vec3& p)
  {
    info.position = p;
  }

  void
  setInfo(NodeInfo* nodeinfo)
  {
    info.position = nodeinfo->position;
    info.rotation_offset = nodeinfo->rotation_offset;
    info.rotation_speed = nodeinfo->rotation_speed;
    info.material = nodeinfo->material;
  }

  void
  create_descriptor_set(const VkDevice& device,
                        const VkDescriptorPool& descriptorPool,
                        const VkDescriptorSetLayout& descriptorSetLayout,
                        VkDescriptorBufferInfo* lightsDescriptor,
                        VkDescriptorBufferInfo* cameraDescriptor,
                        uint32_t eye)
  {
    VkDescriptorSetAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptorSetLayout
    };
    vk_check(
      vkAllocateDescriptorSets(device, &allocInfo, &descriptor_sets[eye]));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = descriptor_sets[eye],
                              .dstBinding = 0,
                              .descriptorCount = 1,
                              .descriptorType =
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                              .pBufferInfo = &uniformBuffer.descriptor },
      (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = descriptor_sets[eye],
                              .dstBinding = 1,
                              .descriptorCount = 1,
                              .descriptorType =
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                              .pBufferInfo = lightsDescriptor },
      (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = descriptor_sets[eye],
                              .dstBinding = 2,
                              .descriptorCount = 1,
                              .descriptorType =
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                              .pBufferInfo = cameraDescriptor }
    };

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(), 0, nullptr);
  }

  void
  update_uniform_buffer(float timer)
  {
    ubo.model = glm::mat4();

    ubo.model = glm::translate(ubo.model, info.position);
    float rotation_z =
      (info.rotation_speed * timer * 360.0f) + info.rotation_offset;
    ubo.model = glm::rotate(ubo.model, glm::radians(rotation_z),
                            glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.normal = glm::inverseTranspose(ubo.model);
    memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
  }

  void
  init_uniform_buffer(vulkan_device* vulkanDevice)
  {
    vulkan_device_create_and_map(vulkanDevice, &uniformBuffer, sizeof(ubo));
  }

  void
  draw(VkCommandBuffer command_buffer,
       VkPipelineLayout pipeline_layout,
       uint32_t eye)
  {
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1, &descriptor_sets[eye], 0,
                            NULL);
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, indexBuffer.buffer, 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdPushConstants(command_buffer, pipeline_layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3),
                       sizeof(Material::PushBlock), &info.material);

    vkCmdDrawIndexed(command_buffer, indexCount, 1, 0, 0, 1);
  }

  int32_t
  newVertex(std::vector<Vertex>* vBuffer,
            float x,
            float y,
            float z,
            const glm::vec3& normal)
  {
    Vertex v(glm::vec3(x, y, z), normal);
    vBuffer->push_back(v);
    return static_cast<int32_t>(vBuffer->size()) - 1;
  }

  void
  newFace(std::vector<uint32_t>* iBuffer, int a, int b, int c)
  {
    iBuffer->push_back(a);
    iBuffer->push_back(b);
    iBuffer->push_back(c);
  }

  void
  generate(vulkan_device* vulkanDevice, GearInfo* gearinfo)
  {
    std::vector<Vertex> vBuffer;
    std::vector<uint32_t> iBuffer;

    int i;
    float r0, r1, r2;
    float ta, da;
    float u1, v1, u2, v2, len;
    float cos_ta, cos_ta_1da, cos_ta_2da, cos_ta_3da, cos_ta_4da;
    float sin_ta, sin_ta_1da, sin_ta_2da, sin_ta_3da, sin_ta_4da;
    int32_t ix0, ix1, ix2, ix3, ix4, ix5;

    r0 = gearinfo->inner_radius;
    r1 = gearinfo->outer_radius - gearinfo->tooth_depth / 2.0f;
    r2 = gearinfo->outer_radius + gearinfo->tooth_depth / 2.0f;
    da = 2.0f * M_PI / gearinfo->tooth_count / 4.0f;

    glm::vec3 normal;

    for (i = 0; i < gearinfo->tooth_count; i++) {
      ta = i * 2.0f * M_PI / gearinfo->tooth_count;

      cos_ta = cos(ta);
      cos_ta_1da = cos(ta + da);
      cos_ta_2da = cos(ta + 2.0f * da);
      cos_ta_3da = cos(ta + 3.0f * da);
      cos_ta_4da = cos(ta + 4.0f * da);
      sin_ta = sin(ta);
      sin_ta_1da = sin(ta + da);
      sin_ta_2da = sin(ta + 2.0f * da);
      sin_ta_3da = sin(ta + 3.0f * da);
      sin_ta_4da = sin(ta + 4.0f * da);

      u1 = r2 * cos_ta_1da - r1 * cos_ta;
      v1 = r2 * sin_ta_1da - r1 * sin_ta;
      len = sqrt(u1 * u1 + v1 * v1);
      u1 /= len;
      v1 /= len;
      u2 = r1 * cos_ta_3da - r2 * cos_ta_2da;
      v2 = r1 * sin_ta_3da - r2 * sin_ta_2da;

      // front face
      normal = glm::vec3(0.0f, 0.0f, 1.0f);
      ix0 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta,
                      gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta,
                      gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta,
                      gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      gearinfo->width * 0.5f, normal);
      ix4 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da,
                      gearinfo->width * 0.5f, normal);
      ix5 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da,
                      gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);
      newFace(&iBuffer, ix2, ix3, ix4);
      newFace(&iBuffer, ix3, ix5, ix4);

      // front sides of teeth
      normal = glm::vec3(0.0f, 0.0f, 1.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta,
                      gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da,
                      gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da,
                      gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      // back face
      normal = glm::vec3(0.0f, 0.0f, -1.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta,
                      -gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta,
                      -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      -gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta,
                      -gearinfo->width * 0.5f, normal);
      ix4 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da,
                      -gearinfo->width * 0.5f, normal);
      ix5 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da,
                      -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);
      newFace(&iBuffer, ix2, ix3, ix4);
      newFace(&iBuffer, ix3, ix5, ix4);

      // back sides of teeth
      normal = glm::vec3(0.0f, 0.0f, -1.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      -gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da,
                      -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta,
                      -gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da,
                      -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      // draw outward faces of teeth
      normal = glm::vec3(v1, -u1, 0.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta,
                      gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta,
                      -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da,
                      gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da,
                      -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      normal = glm::vec3(cos_ta, sin_ta, 0.0f);
      ix0 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da,
                      gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da,
                      -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da,
                      gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da,
                      -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      normal = glm::vec3(v2, -u2, 0.0f);
      ix0 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da,
                      gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da,
                      -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      normal = glm::vec3(cos_ta, sin_ta, 0.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da,
                      -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da,
                      gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da,
                      -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      // draw inside radius cylinder
      ix0 =
        newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, -gearinfo->width * 0.5f,
                  glm::vec3(-cos_ta, -sin_ta, 0.0f));
      ix1 =
        newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, gearinfo->width * 0.5f,
                  glm::vec3(-cos_ta, -sin_ta, 0.0f));
      ix2 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da,
                      -gearinfo->width * 0.5f,
                      glm::vec3(-cos_ta_4da, -sin_ta_4da, 0.0f));
      ix3 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da,
                      gearinfo->width * 0.5f,
                      glm::vec3(-cos_ta_4da, -sin_ta_4da, 0.0f));
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);
    }

    size_t vertexBufferSize = vBuffer.size() * sizeof(Vertex);
    size_t indexBufferSize = iBuffer.size() * sizeof(uint32_t);

    // Vertex buffer
    vulkan_device_create_buffer(vulkanDevice, &vertexBuffer,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                vertexBufferSize, (void*)vBuffer.data());
    // Index buffer
    vulkan_device_create_buffer(vulkanDevice, &indexBuffer,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                indexBufferSize, (void*)iBuffer.data());

    indexCount = iBuffer.size();
  }
};
