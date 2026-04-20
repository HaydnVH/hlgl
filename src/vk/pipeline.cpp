#include "pipeline.h"
#include "context.h"
#include "shader.h"

#include "../utils/array.h"
#include <vector>
#include <map>
#include <string>
#include <string_view>

hlgl::Pipeline::Pipeline(Pipeline::ComputeParams params)
: _pimpl(std::make_unique<PipelineImpl>(std::move(params)))
{ if (!_pimpl->pipeline) _pimpl.reset(); }

hlgl::Pipeline::Pipeline(Pipeline::GraphicsParams params)
: _pimpl(std::make_unique<PipelineImpl>(std::move(params)))
{ if (!_pimpl->pipeline) _pimpl.reset(); }

hlgl::PipelineImpl::PipelineImpl(Pipeline::ComputeParams&& params)
{
  // Assemble shaders and stages.
  Array<ShaderInfo,8> shaders;
  shaders.push_back(params.compShader);
  if (!initLayout(shaders, VK_SHADER_STAGE_COMPUTE_BIT)) {
    return;
  }

  // Create the pipeline.
  VkComputePipelineCreateInfo pci {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = params.compShader.shader->_pimpl->module,
      .pName = params.compShader.entry },
    .layout = layout };
  if (!VKCHECK(vkCreateComputePipelines(getDevice(), nullptr, 1, &pci, nullptr, &pipeline)) || !pipeline) {
    DEBUG_ERROR("Failed to create compute pipeline.");
    return;
  }

  // Set the debug name.
  if ((isValidationEnabled()) && params.debugName) {
    VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = VK_OBJECT_TYPE_PIPELINE;
    info.objectHandle = (uint64_t)pipeline;
    info.pObjectName = params.debugName;
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info))) {
      DEBUG_WARNING("Failed to set vulkan debug name for '%s'.", params.debugName);
    }
  }
}

hlgl::PipelineImpl::PipelineImpl(Pipeline::GraphicsParams&& params)
{

  // Assemble shaders and stages.
  Array<ShaderInfo,8> shaders;
  VkShaderStageFlags stages {0};
  if (params.vertShader.shader) { shaders.push_back({params.vertShader.shader, params.vertShader.entry, ShaderStage::Vertex});          stages |= VK_SHADER_STAGE_VERTEX_BIT; }
  if (params.geomShader.shader) { shaders.push_back({params.geomShader.shader, params.geomShader.entry, ShaderStage::Geometry});        stages |= VK_SHADER_STAGE_GEOMETRY_BIT; }
  if (params.tescShader.shader) { shaders.push_back({params.tescShader.shader, params.tescShader.entry, ShaderStage::TessControl});     stages |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; }
  if (params.teseShader.shader) { shaders.push_back({params.teseShader.shader, params.teseShader.entry, ShaderStage::TessEvaluation});  stages |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; }
  if (params.fragShader.shader) { shaders.push_back({params.fragShader.shader, params.fragShader.entry, ShaderStage::Fragment});        stages |= VK_SHADER_STAGE_FRAGMENT_BIT; }
  if (params.taskShader.shader) { shaders.push_back({params.taskShader.shader, params.taskShader.entry, ShaderStage::Task});            stages |= VK_SHADER_STAGE_TASK_BIT_EXT; }
  if (params.meshShader.shader) { shaders.push_back({params.meshShader.shader, params.meshShader.entry, ShaderStage::Mesh});            stages |= VK_SHADER_STAGE_MESH_BIT_EXT; }
  if (!initLayout(shaders, stages)) {
    return;
  }

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  shaderStages.reserve(shaders.size());
  for (const ShaderInfo& info : shaders) {
    shaderStages.push_back(VkPipelineShaderStageCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = translate(info.stage),
      .module = info.shader->_pimpl->module,
      .pName = info.entry
    });
  }

  // Define all the create infos that go into a graphics pipeline.
  // Most of these COULD be replaced with dynamic state in modern VK, but the usefulness of doing so is debatable imo.
  VkPipelineVertexInputStateCreateInfo vertexInput {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, };
  VkPipelineInputAssemblyStateCreateInfo inputAssembly {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = translate(params.primitive),
    .primitiveRestartEnable = params.primitiveRestart };
  VkPipelineViewportStateCreateInfo viewport {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1 };
  VkPipelineRasterizationStateCreateInfo raster {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = false,
    .rasterizerDiscardEnable = false,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = translate(params.cullMode),
    .frontFace = params.frontFace == FrontFace::CounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = (params.depthAttachment && params.depthAttachment->bias),
    .depthBiasConstantFactor = (params.depthAttachment && params.depthAttachment->bias) ? params.depthAttachment->bias->constFactor : 0.0f,
    .depthBiasClamp = (params.depthAttachment && params.depthAttachment->bias) ? params.depthAttachment->bias->clamp : 0.0f,
    .depthBiasSlopeFactor = (params.depthAttachment && params.depthAttachment->bias) ? params.depthAttachment->bias->slopeFactor : 0.0f,
    .lineWidth = 1.0f };
  VkPipelineMultisampleStateCreateInfo msaa {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = translateMsaa(params.msaa),
    .sampleShadingEnable = (params.msaa > 1) };
  VkPipelineDepthStencilStateCreateInfo depthStencil {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = (params.depthAttachment) ? params.depthAttachment->test : false,
    .depthWriteEnable = (params.depthAttachment) ? params.depthAttachment->write : false,
    .depthCompareOp = (params.depthAttachment) ? translate(params.depthAttachment->compare) : VK_COMPARE_OP_ALWAYS,
    .depthBoundsTestEnable = false,
    .stencilTestEnable = false,
    .front = {},
    .back = {},
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f };
  std::vector<VkPipelineColorBlendAttachmentState> colorAttachmentBlends;
  std::vector<VkFormat> colorAttachmentFormats;
  for (const ColorAttachmentInfo& attachment : params.colorAttachments) {
    colorAttachmentBlends.push_back(attachment.blending ? 
      VkPipelineColorBlendAttachmentState {
        .blendEnable = true,
        .srcColorBlendFactor = translate(attachment.blending->srcColorFactor),
        .dstColorBlendFactor = translate(attachment.blending->dstColorFactor),
        .colorBlendOp = translate(attachment.blending->colorOp),
        .srcAlphaBlendFactor = translate(attachment.blending->srcAlphaFactor),
        .dstAlphaBlendFactor = translate(attachment.blending->dstAlphaFactor),
        .alphaBlendOp = translate(attachment.blending->alphaOp),
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_B_BIT } :
      VkPipelineColorBlendAttachmentState {
        .blendEnable = false,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_B_BIT});
    if (attachment.blending)
      isOpaque = false;
    colorAttachmentFormats.push_back(translate(attachment.format));
  }
  VkPipelineColorBlendStateCreateInfo colorBlend {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = false,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = (uint32_t)colorAttachmentBlends.size(),
    .pAttachments = colorAttachmentBlends.data() };
  std::array dynamicStates {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = dynamicStates.size(),
    .pDynamicStates = dynamicStates.data() };
  VkPipelineRenderingCreateInfo render {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
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
    .layout = layout,
    .renderPass = nullptr,
    .subpass = 0,
    .basePipelineHandle = nullptr,
    .basePipelineIndex = -1 };
  if (!VKCHECK(vkCreateGraphicsPipelines(getDevice(), nullptr, 1, &pci, nullptr, &pipeline)) || !pipeline) {
    DEBUG_ERROR("Failed to create graphics pipeline.");
    return;
  }

  // Set the debug name.
  if (isValidationEnabled() && params.debugName) {
    VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = VK_OBJECT_TYPE_PIPELINE;
    info.objectHandle = (uint64_t)pipeline;
    info.pObjectName = params.debugName;
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
      DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", params.debugName);
  }
}

hlgl::Pipeline::~Pipeline() {
  if (!_pimpl) return;
  if (_pimpl->pipeline || _pimpl->layout)
    queueDeletion(DelQueuePipeline{.pipeline = _pimpl->pipeline, .layout = _pimpl->layout});
}

bool hlgl::Pipeline::isCompute() const { return (_pimpl && (_pimpl->bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)); }
bool hlgl::Pipeline::isGraphics() const { return (_pimpl && (_pimpl->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)); }

bool hlgl::PipelineImpl::initLayout(const hlgl::Array<hlgl::ShaderInfo,8>& shaders, VkShaderStageFlags stages) {
  if (shaders.size() == 0) {
    DEBUG_ERROR("No shaders provided for pipeline creation.");
    return false;
  }

  // Merge push constants.
  for (const ShaderInfo& info : shaders) {
    if (info.shader->_pimpl->pushConstants.stageFlags) {
      if (!pushConstRange.stageFlags) {
        pushConstRange = VkPushConstantRange{
          .offset = info.shader->_pimpl->pushConstants.offset,
          .size = info.shader->_pimpl->pushConstants.size
        };
      }
      if (pushConstRange.offset != info.shader->_pimpl->pushConstants.offset) {
        DEBUG_ERROR("Shader push constant offset mismatch.");
        return false;
      }
      if (pushConstRange.size != info.shader->_pimpl->pushConstants.size) {
        DEBUG_ERROR("Shader push constant size mismatch.");
        return false;
      }
      pushConstRange.stageFlags |= info.shader->_pimpl->pushConstants.stageFlags;
    }
  }

  // Use the descriptor set layout and push constant range to create the pipeline layout.
  VkPipelineLayoutCreateInfo plci {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = (uint32_t)getDescSetLayouts().size(),
    .pSetLayouts = getDescSetLayouts().data(),
    .pushConstantRangeCount = (uint32_t)((pushConstRange.stageFlags == 0) ? 0 : 1),
    .pPushConstantRanges = ((pushConstRange.stageFlags == 0) ? nullptr : &pushConstRange) };
  if (!VKCHECK(vkCreatePipelineLayout(getDevice(), &plci, nullptr, &layout)) || !layout) {
    DEBUG_ERROR("Failed to create pipeline layout.");
    return false;
  }

  return true;
}