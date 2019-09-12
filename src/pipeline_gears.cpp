/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <array>

#include "pipeline_gears.hpp"

#include "vulkan_shader.hpp"

#include "gears.frag.h"
#include "gears.vert.h"

typedef enum Component
{
  VERTEX_COMPONENT_POSITION = 0x0,
  VERTEX_COMPONENT_NORMAL = 0x1,
  VERTEX_COMPONENT_COLOR = 0x2,
  VERTEX_COMPONENT_UV = 0x3,
  VERTEX_COMPONENT_TANGENT = 0x4,
  VERTEX_COMPONENT_BITANGENT = 0x5,
  VERTEX_COMPONENT_DUMMY_FLOAT = 0x6,
  VERTEX_COMPONENT_DUMMY_VEC4 = 0x7
} Component;

struct VertexLayout
{
public:
  /** @brief Components used to generate vertices from */
  std::vector<Component> components;

  explicit VertexLayout(std::vector<Component> components)
  {
    this->components = std::move(components);
  }

  uint32_t
  stride()
  {
    uint32_t res = 0;
    for (auto& component : components) {
      switch (component) {
      case VERTEX_COMPONENT_UV: res += 2 * sizeof(float); break;
      case VERTEX_COMPONENT_DUMMY_FLOAT: res += sizeof(float); break;
      case VERTEX_COMPONENT_DUMMY_VEC4: res += 4 * sizeof(float); break;
      default:
        // All components except the ones listed above are made up of 3 floats
        res += 3 * sizeof(float);
      }
    }
    return res;
  }
};


pipeline_gears::pipeline_gears(vulkan_device* vulkan_device,
                               VkRenderPass render_pass,
                               VkPipelineCache pipeline_cache,
                               VkDescriptorBufferInfo* camera_descriptor[2])
{
  this->device = vulkan_device->device;

  init_gears(vulkan_device);
  init_uniform_buffers(vulkan_device);
  init_descriptor_pool();
  init_descriptor_set_layout();
  init_pipeline(render_pass, pipeline_cache);
  for (uint32_t i = 0; i < 2; i++)
    init_descriptor_sets(i, camera_descriptor[i]);
}

pipeline_gears::~pipeline_gears()
{
  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

  uniform_buffers.lights.destroy();

  for (auto& node : nodes)
    delete (node);

  vkDestroyPipeline(device, pipeline, nullptr);

  vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
}

void
pipeline_gears::draw(VkCommandBuffer command_buffer, uint32_t eye)
{
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  for (auto& node : nodes)
    node->draw(command_buffer, pipeline_layout, eye);
}

void
pipeline_gears::init_gears(vulkan_device* vk_device)
{
  // Gear definitions
  std::vector<float> inner_radiuses = { 1.0f, 0.5f, 1.3f };
  std::vector<float> outer_radiuses = { 4.0f, 2.0f, 2.0f };
  std::vector<float> widths = { 1.0f, 2.0f, 0.5f };
  std::vector<int32_t> tooth_count = { 20, 10, 10 };
  std::vector<float> tooth_depth = { 0.7f, 0.7f, 0.7f };
  std::vector<glm::vec3> positions = { glm::vec3(-3.0, 0.0, -20.0),
                                       glm::vec3(3.1, 0.0, -20.0),
                                       glm::vec3(-3.1, -6.2, -20.0) };

  std::vector<Material> materials = {
    Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 0.3f, 0.7f),
    Material("Green", glm::vec3(0.0f, 1.0f, 0.2f), 0.3f, 0.7f),
    Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.3f, 0.7f)
  };

  std::vector<float> rotation_speeds = { 1.0f, -2.0f, -2.0f };
  std::vector<float> rotation_offsets = { 0.0f, -9.0f, -30.0f };

  nodes.resize(positions.size());
  for (uint32_t i = 0; i < nodes.size(); ++i) {

    GearInfo gear_info = { .inner_radius = inner_radiuses[i],
                           .outer_radius = outer_radiuses[i],
                           .width = widths[i],
                           .tooth_count = tooth_count[i],
                           .tooth_depth = tooth_depth[i] };

    Gear::NodeInfo gear_node_info = { .position = positions[i],
                                      .rotation_speed = rotation_speeds[i],
                                      .rotation_offset = rotation_offsets[i],
                                      .material = materials[i] };

    nodes[i] = new Gear();
    nodes[i]->setInfo(&gear_node_info);
    ((Gear*)nodes[i])->generate(vk_device, &gear_info);
  }
}

void
pipeline_gears::init_descriptor_pool()
{
  // Example uses two ubos
  std::vector<VkDescriptorPoolSize> pool_sizes = {
    { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 34 },
    { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 6 }
  };

  VkDescriptorPoolCreateInfo descriptor_pool_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 12,
    .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data()
  };
  vk_check(vkCreateDescriptorPool(device, &descriptor_pool_info, nullptr,
                                  &descriptor_pool));
}

void
pipeline_gears::init_descriptor_set_layout()
{
  std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
    // ubo model
    { .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT },
    // ubo lights
    { .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
    // ubo camera
    { .binding = 2,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
  };

  VkDescriptorSetLayoutCreateInfo descriptor_layout = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = static_cast<uint32_t>(set_layout_bindings.size()),
    .pBindings = set_layout_bindings.data()
  };

  vk_check(vkCreateDescriptorSetLayout(device, &descriptor_layout, nullptr,
                                       &descriptor_set_layout));
  /*
   * Push Constants
   */
  std::vector<VkPushConstantRange> push_constant_ranges = {
    { .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = sizeof(glm::vec3),
      .size = sizeof(Material::PushBlock) }
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &descriptor_set_layout,
    .pushConstantRangeCount = (uint32_t)push_constant_ranges.size(),
    .pPushConstantRanges = push_constant_ranges.data()
  };
  vk_check(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                                  &pipeline_layout));
}

void
pipeline_gears::init_descriptor_sets(uint32_t eye,
                                     VkDescriptorBufferInfo* camera_descriptor)
{
  for (auto& node : nodes)
    node->create_descriptor_set(device, descriptor_pool, descriptor_set_layout,
                                &uniform_buffers.lights.descriptor,
                                camera_descriptor, eye);
}

void
pipeline_gears::init_pipeline(VkRenderPass render_pass,
                              VkPipelineCache pipeline_cache)
{
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE
  };

  VkPipelineRasterizationStateCreateInfo rasterization_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .lineWidth = 1.0f
  };

  VkPipelineColorBlendAttachmentState blend_attachment_state = {
    .blendEnable = VK_FALSE, .colorWriteMask = 0xf
  };

  VkPipelineColorBlendStateCreateInfo color_blend_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &blend_attachment_state
  };

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    .front = { .compareOp = VK_COMPARE_OP_ALWAYS },
    .back = { .compareOp = VK_COMPARE_OP_ALWAYS }
  };

  VkPipelineViewportStateCreateInfo viewport_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = static_cast<uint32_t>(1),
    .scissorCount = static_cast<uint32_t>(1)
  };

  VkPipelineMultisampleStateCreateInfo multisample_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
  };

  std::vector<VkDynamicState> dynamic_state_enables = {
    VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_LINE_WIDTH
  };
  VkPipelineDynamicStateCreateInfo dynamic_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(dynamic_state_enables.size()),
    .pDynamicStates = dynamic_state_enables.data()
  };

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
    vulkan_shader::load(device, shaders_gears_vert, sizeof(shaders_gears_vert),
                        VK_SHADER_STAGE_VERTEX_BIT),
    vulkan_shader::load(device, shaders_gears_frag, sizeof(shaders_gears_frag),
                        VK_SHADER_STAGE_FRAGMENT_BIT)
  };

  // Vertex layout for the models
  VertexLayout vertex_layout =
    VertexLayout({ VERTEX_COMPONENT_POSITION, VERTEX_COMPONENT_NORMAL });

  // Vertex bindings an attributes
  std::vector<VkVertexInputBindingDescription> vertex_input_bindings = {
    { .binding = 0,
      .stride = vertex_layout.stride(),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }
  };
  std::vector<VkVertexInputAttributeDescription> vertex_input_attributes = {
    // Location 0: Position
    { .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = 0 },
    // Location 1: Normals
    { .location = 1,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = sizeof(float) * 3 },
    // Location 2: Color
    { .location = 2,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = sizeof(float) * 6 }
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount =
      static_cast<uint32_t>(vertex_input_bindings.size()),
    .pVertexBindingDescriptions = vertex_input_bindings.data(),
    .vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertex_input_attributes.size()),
    .pVertexAttributeDescriptions = vertex_input_attributes.data()
  };

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = static_cast<uint32_t>(shader_stages.size()),
    .pStages = shader_stages.data(),
    .pVertexInputState = &vertex_input_state,
    .pInputAssemblyState = &input_assembly_state,
    .pViewportState = &viewport_state,
    .pRasterizationState = &rasterization_state,
    .pMultisampleState = &multisample_state,
    .pDepthStencilState = &depth_stencil_state,
    .pColorBlendState = &color_blend_state,
    .pDynamicState = &dynamic_state,
    .layout = pipeline_layout,
    .basePipelineHandle = nullptr,
    .basePipelineIndex = -1,
  };


  // only needs to be compatible
  pipeline_info.renderPass = render_pass;

  vk_check(vkCreateGraphicsPipelines(device, pipeline_cache, 1, &pipeline_info,
                                     nullptr, &pipeline));

  vkDestroyShaderModule(device, shader_stages[0].module, nullptr);
  vkDestroyShaderModule(device, shader_stages[1].module, nullptr);
}

void
pipeline_gears::update_lights()
{
  ubo_lights.lights[0] = glm::vec4(-5, -10, 15, 1.0f);
  ubo_lights.lights[1] = glm::vec4(5, -10, 10, 1.0f);
  ubo_lights.lights[2] = glm::vec4(0, 5, 15, 1.0f);
  ubo_lights.lights[3] = glm::vec4(-10, -20, 15, 1.0f);

  memcpy(uniform_buffers.lights.mapped, &ubo_lights, sizeof(ubo_lights));
}

void
pipeline_gears::update_uniform_buffers(float animation_timer)
{
  for (Gear* node : nodes)
    node->update_uniform_buffer(animation_timer);

  update_lights();
}

void
pipeline_gears::init_uniform_buffers(vulkan_device* vk_device)
{
  vk_device->create_and_map(&uniform_buffers.lights, sizeof(ubo_lights));

  for (auto& node : nodes)
    node->init_uniform_buffer(vk_device);
}
