#include "vk-includes.h"
#include "vk-debug.h"
#include "vk-translate.h"
#include <hlgl/core/context.h>
#include <hlgl/core/pipeline.h>

#include <fmt/format.h>

#include <vector>
#include <map>
#include <string>
#include <string_view>

hlgl::ComputePipeline::ComputePipeline(Context& context, ComputePipelineParams params)
: Pipeline(context)
{
  if (!initShaders({params.shader}))
    return;

  VkComputePipelineCreateInfo pci {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = params.shader->shader_,
      .pName = params.shader->entry_ },
    .layout = layout_ };
  if (!VKCHECK(vkCreateComputePipelines(context_.device_, nullptr, 1, &pci, nullptr, &pipeline_)) || !pipeline_) {
    debugPrint(DebugSeverity::Error, "Failed to create compute pipeline.");
    return;
  }

  // Set the debug name.
  if ((context_.gpu_.enabledFeatures & Feature::Validation) && params.sDebugName) {
    VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = VK_OBJECT_TYPE_PIPELINE;
    info.objectHandle = (uint64_t)pipeline_;
    info.pObjectName = params.sDebugName;
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(context_.device_, &info)))
      return;
  }

  initSuccess_ = true;
}

hlgl::GraphicsPipeline::GraphicsPipeline(Context& context, GraphicsPipelineParams params)
: Pipeline(context)
{
  if (!initShaders(params.shaders))
    return;

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  shaderStages.reserve(params.shaders.size());
  for (Shader* shader : params.shaders) {
    shaderStages.push_back(VkPipelineShaderStageCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = (VkShaderStageFlagBits)shader->stage_,
      .module = shader->shader_,
      .pName = shader->entry_ });
  }
  VkPipelineVertexInputStateCreateInfo vertexInput {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, };
  VkPipelineInputAssemblyStateCreateInfo inputAssembly {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = translate(params.ePrimitive),
    .primitiveRestartEnable = params.bPrimitiveRestart };
  VkPipelineViewportStateCreateInfo viewport {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1 };
  VkPipelineRasterizationStateCreateInfo raster {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = false,
    .rasterizerDiscardEnable = false,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = translate(params.eCullMode),
    .frontFace = params.eFrontFace == FrontFace::CounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = (params.depthAttachment && params.depthAttachment->bias),
    .depthBiasConstantFactor = (params.depthAttachment && params.depthAttachment->bias) ? params.depthAttachment->bias->fConst : 0.0f,
    .depthBiasClamp = (params.depthAttachment && params.depthAttachment->bias) ? params.depthAttachment->bias->fClamp : 0.0f,
    .depthBiasSlopeFactor = (params.depthAttachment && params.depthAttachment->bias) ? params.depthAttachment->bias->fSlope : 0.0f,
    .lineWidth = 1.0f };
  VkPipelineMultisampleStateCreateInfo msaa {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = translateMsaa(params.iMsaa),
    .sampleShadingEnable = (translateMsaa(params.iMsaa) != VK_SAMPLE_COUNT_1_BIT) };
  VkPipelineDepthStencilStateCreateInfo depthStencil {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = (params.depthAttachment) ? params.depthAttachment->bTest : false,
    .depthWriteEnable = (params.depthAttachment) ? params.depthAttachment->bWrite : false,
    .depthCompareOp = (params.depthAttachment) ? translate(params.depthAttachment->eCompare) : VK_COMPARE_OP_ALWAYS,
    .depthBoundsTestEnable = false,
    .stencilTestEnable = false,
    .front = {},
    .back = {},
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f };
  std::vector<VkPipelineColorBlendAttachmentState> colorAttachmentBlends;
  std::vector<VkFormat> colorAttachmentFormats;
  for (auto& attachment : params.colorAttachments) {
    colorAttachmentBlends.push_back(attachment.blend ?
      VkPipelineColorBlendAttachmentState{
        .blendEnable = true,
        .srcColorBlendFactor = translate(attachment.blend->srcColorFactor),
        .dstColorBlendFactor = translate(attachment.blend->dstColorFactor),
        .colorBlendOp        = translate(attachment.blend->colorOp),
        .srcAlphaBlendFactor = translate(attachment.blend->srcAlphaFactor),
        .dstAlphaBlendFactor = translate(attachment.blend->dstAlphaFactor),
        .alphaBlendOp        = translate(attachment.blend->alphaOp),
        .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT |
          VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT |
          VK_COLOR_COMPONENT_A_BIT } :
      VkPipelineColorBlendAttachmentState{
        .blendEnable = false,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT |  
          VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT |
          VK_COLOR_COMPONENT_A_BIT });
    if (colorAttachmentBlends.back().blendEnable)
      isOpaque_ = false;
    colorAttachmentFormats.push_back(translate(attachment.format));
  }
  VkPipelineColorBlendStateCreateInfo colorBlend {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = false,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = (uint32_t)colorAttachmentBlends.size(),
    .pAttachments = colorAttachmentBlends.data() };
  std::array dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = (uint32_t)dynamicStates.size(),
    .pDynamicStates = dynamicStates.data() };
  VkPipelineRenderingCreateInfoKHR render {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .colorAttachmentCount = (uint32_t)colorAttachmentFormats.size(),
    .pColorAttachmentFormats = colorAttachmentFormats.data(),
    .depthAttachmentFormat = (params.depthAttachment) ? translate(params.depthAttachment->format) : VK_FORMAT_UNDEFINED };

  VkGraphicsPipelineCreateInfo pci {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &render,
    .stageCount = (uint32_t)shaderStages.size(),
    .pStages = shaderStages.data(),
    .pVertexInputState = &vertexInput,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewport,
    .pRasterizationState = &raster,
    .pMultisampleState = &msaa,
    .pDepthStencilState = &depthStencil,
    .pColorBlendState = &colorBlend,
    .pDynamicState = &dynamic,
    .layout = layout_,
    .renderPass = nullptr,
    .subpass = 0,
    .basePipelineHandle = nullptr,
    .basePipelineIndex = -1 };
  if (!VKCHECK(vkCreateGraphicsPipelines(context_.device_, nullptr, 1, &pci, nullptr, &pipeline_))  || !pipeline_) {
    debugPrint(DebugSeverity::Error, "Failed to create graphics pipeline.");
    return;
  }

  // Set the debug name.
  if ((context_.gpu_.enabledFeatures & Feature::Validation) && params.sDebugName) {
    VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = VK_OBJECT_TYPE_PIPELINE;
    info.objectHandle = (uint64_t)pipeline_;
    info.pObjectName = params.sDebugName;
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(context_.device_, &info)))
      return;
  }

  initSuccess_ = true;
}

hlgl::Pipeline::~Pipeline() {
  context_.queueDeletion(Context::DelQueuePipeline{.pipeline = pipeline_, .layout = layout_, .descLayout = descLayout_});
}

bool hlgl::Pipeline::initShaders(const std::initializer_list<Shader*>& shaders) {
  // Create shader modules.
  VkShaderStageFlags stages {0};
  //std::vector<ShaderModule> shaders;
  //shaders.reserve(shaders.size());
  for (Shader* shader : shaders) {
    // If this shader parameter is invalid, skip it.
    if (!shader)
      continue;

    if (!shader->isValid())
      return false;

    stages |= shader->stage_;
  }

  if (shaders.size() == 0 || stages == 0) {
    debugPrint(DebugSeverity::Error, "No valid shaders for pipeline.");
    return false;
  }

  // Make sure the correct shader stages are present.
  bool isCompute = (bool)(stages & VK_SHADER_STAGE_COMPUTE_BIT);
  if (isCompute && !(stages == VK_SHADER_STAGE_COMPUTE_BIT)) {
    debugPrint(DebugSeverity::Error, "Pipeline must be compute (only a compute shader) or graphics (only non-compute shaders).");
    return false;
  }

  if (!isCompute && !(stages & VK_SHADER_STAGE_FRAGMENT_BIT)) {
    debugPrint(DebugSeverity::Error, "Graphics pipeline must include a fragment shader.");
    return false;
  }

  if (!isCompute && !(stages & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT))) {
    debugPrint(DebugSeverity::Error, "Graphics pipeline must include a vertex or mesh shader.");
    return false;
  }

  type_ = (isCompute) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

  // Merge descriptor set layout bindings.
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
  uint32_t highestBinding {0};
  // For each shader...
  for (Shader* shader : shaders) {
    // Go through each layout binding in that shader...
    for (auto& shaderBinding : shader->layoutBindings_) {
      bool updated {false};
      // Try to find the same binding from a previous shader.
      for (auto& existingBinding : layoutBindings) {
        if (existingBinding.binding == shaderBinding.binding) {
          // If we find it, update the binding's stage flags.
          existingBinding.stageFlags |= shaderBinding.stageFlags;
          updated = true;
          break;
        }
      }
      // If we updated the binding's stage flags, there's no more to do with this shader binding.
      if (updated)
        continue;
      // If we didn't, that means this binding is new, so it's added to the list.
      else {
        layoutBindings.push_back(shaderBinding);
        highestBinding = std::max(highestBinding, shaderBinding.binding);
      }
    }
  }

  // Create the descriptor set layout.
  VkDescriptorSetLayoutCreateInfo dslci {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
    .bindingCount = (uint32_t)layoutBindings.size(),
    .pBindings = layoutBindings.data() };

  if (!VKCHECK(vkCreateDescriptorSetLayout(context_.device_, &dslci, nullptr, &descLayout_)) || !descLayout_) {
    debugPrint(DebugSeverity::Error, "Failed to create descriptor set layout.");
    return false;
  }


  // Save descriptor binding types.
  descTypes_.resize(highestBinding+1);
  for (auto& binding : layoutBindings) {
    descTypes_[binding.binding] = binding.descriptorType;
  }

  // Merge push constants.
  for (Shader* shader : shaders) {
    if (shader->pushConstants_.stageFlags) {
      if (!pushConstRange_.stageFlags) {
        pushConstRange_ = VkPushConstantRange{
          .offset = shader->pushConstants_.offset,
          .size = shader->pushConstants_.size
        };
      }
      if (pushConstRange_.offset != shader->pushConstants_.offset) {
        debugPrint(DebugSeverity::Error, "Shader push constant offset mismatch.");
        return false;
      }
      if (pushConstRange_.size != shader->pushConstants_.size) {
        debugPrint(DebugSeverity::Error, "Shader push constant size mismatch.");
        return false;
      }
      pushConstRange_.stageFlags |= shader->pushConstants_.stageFlags;
    }
  }

  // Use the descriptor set layout and push constant range to create the pipeline layout.
  VkPipelineLayoutCreateInfo plci {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &descLayout_,
    .pushConstantRangeCount = (uint32_t)((pushConstRange_.stageFlags == 0) ? 0 : 1),
    .pPushConstantRanges = ((pushConstRange_.stageFlags == 0) ? nullptr : &pushConstRange_) };
  if (!VKCHECK(vkCreatePipelineLayout(context_.device_, &plci, nullptr, &layout_)) || !layout_) {
    debugPrint(DebugSeverity::Error, "Failed to create pipeline layout.");
    return false;
  }

  return true;
}