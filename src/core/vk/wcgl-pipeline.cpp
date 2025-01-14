#include "wcgl-vk-includes.h"
#include "wcgl-vk-debug.h"
#include "wcgl-vk-translate.h"
#include "wcgl/core/wcgl-pipeline.h"

#include <fmt/format.h>
#include <shaderc/shaderc.hpp>
#include <spirv_reflect.h>

#include <vector>
#include <map>
#include <string>
#include <string_view>


namespace {

class Shader {
public:
  Shader(VkDevice device, wcgl::ShaderParams params)
    : device(device)
  {
    using namespace wcgl;
    std::vector<uint32_t> spvCompiled;
    const void* spvSrc {nullptr};
    size_t spvSize {0};

    // If the user provides a pointer and size for Spir-V, simply use that.
    if (params.pSpv && params.iSpvSize) {
      spvSrc = params.pSpv;
      spvSize = params.iSpvSize;
    }
    // If GLSL source code is provided, use shaderc to compile it to Spir-V.
    else if (params.sGlsl != "") {
      shaderc::Compiler compiler;
      shaderc::CompileOptions options;
      options.SetOptimizationLevel(shaderc_optimization_level_performance);

      shaderc_shader_kind kind;
      std::string_view nameView{params.sName};
      if (nameView.find(".vert") != std::string_view::npos) kind = shaderc_glsl_vertex_shader;
      else if (nameView.find(".frag") != std::string_view::npos) kind = shaderc_glsl_fragment_shader;
      else if (nameView.find(".geom") != std::string_view::npos) kind = shaderc_glsl_geometry_shader;
      else if (nameView.find(".tesc") != std::string_view::npos) kind = shaderc_glsl_tess_control_shader;
      else if (nameView.find(".tese") != std::string_view::npos) kind = shaderc_glsl_tess_evaluation_shader;
      else if (nameView.find(".comp") != std::string_view::npos) kind = shaderc_glsl_compute_shader;
      else if (nameView.find(".task") != std::string_view::npos) kind = shaderc_glsl_task_shader;
      else if (nameView.find(".mesh") != std::string_view::npos) kind = shaderc_glsl_mesh_shader;
      else kind = shaderc_glsl_infer_from_source;

      std::string_view glsl {params.sGlsl};
      shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(glsl.data(), glsl.size(), kind, params.sName, params.sEntry, options);
      if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        debugPrint(DebugSeverity::Error, fmt::format("Failed to compile shader: {}", result.GetErrorMessage()));
        return;
      }
      spvCompiled = {result.cbegin(), result.cend()};
      spvSrc = (const void*)spvCompiled.data();
      spvSize = spvCompiled.size() * sizeof(uint32_t);
    }
    // TODO: Figure out how to compile HLSL to Spir-V.  I know shaderc can do it, but the documentation on it is unclear.
    //else if (params.sHlsl != "") {
    //  wcgl::debugPrint(wcgl::DebugSeverity::Error, "HLSL shader support is not currently implemented.");
    //  return;
    //}

    if (!spvSrc || !spvSize) {
      debugPrint(DebugSeverity::Error, "No shader source provided.");
      return;
    }

    // Create the Spirv-Reflect shader module.
    SpvReflectShaderModule spvModule;
    if (spvReflectCreateShaderModule(spvSize, spvSrc, &spvModule) != SPV_REFLECT_RESULT_SUCCESS) {
      debugPrint(DebugSeverity::Error, "Failed to create SpirV-Reflect shader module.");
      return;
    }

    // Create the vulkan shader module.
    VkShaderModuleCreateInfo ci {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = (uint32_t)spvSize,
      .pCode = (uint32_t*)spvSrc };
    if (!VKCHECK(vkCreateShaderModule(device, &ci, nullptr, &shader)) || !shader) {
      debugPrint(DebugSeverity::Error, "Failed to create shader module.");
      return;
    }
    stage = spvModule.shader_stage;
    entry = params.sEntry;

    // Get the push constant range.
    if (spvModule.push_constant_block_count > 1)
      wcgl::debugPrint(wcgl::DebugSeverity::Warning, "Can't create a shader with more than one push constant block.");
    if (spvModule.push_constant_block_count > 0 && spvModule.push_constant_blocks) {
      pushConstants.stageFlags = stage;
      pushConstants.offset = spvModule.push_constant_blocks->offset;
      pushConstants.size = spvModule.push_constant_blocks->size;
    }

    // Get descriptor set layout bindings.
    uint32_t spvBindingCount {0};
    if (spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, nullptr) != SPV_REFLECT_RESULT_SUCCESS) return;
    std::vector<SpvReflectDescriptorBinding*> spvBindings(spvBindingCount);
    if (spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, spvBindings.data()) != SPV_REFLECT_RESULT_SUCCESS) return;

    layoutBindings.reserve(spvBindingCount);
    for (uint32_t i {0}; i < spvBindingCount; ++i) {
      layoutBindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = spvBindings[i]->binding,
        .descriptorType = (VkDescriptorType)spvBindings[i]->descriptor_type,
        .descriptorCount = 1,
        .stageFlags = stage
        });
    }

    spvReflectDestroyShaderModule(&spvModule);
  }
  ~Shader() {
    if (shader)
      vkDestroyShaderModule(device, shader, nullptr);
  }
  operator bool() const { return (shader); }

  VkDevice device {nullptr};
  VkShaderModule shader {nullptr};
  VkShaderStageFlags stage {};
  const char* entry {"main"};
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings {};
  VkPushConstantRange pushConstants {};
};

} // namespace <anon>

wcgl::Pipeline::Pipeline(const Context& context, PipelineParams params)
: context_(context)
{
  // Create shader modules.
  VkShaderStageFlags stages {0};
  std::vector<Shader> shaders;
  shaders.reserve(params.shaders.size());
  for (auto& shaderParam : params.shaders) {
    if (shaderParam.sName == nullptr) {
      debugPrint(DebugSeverity::Error, "Shader name cannot be null.");
      return;
    }
    shaders.emplace_back(context_.device_, shaderParam);
    if (!shaders.back())
      return;

    stages |= shaders.back().stage;
  }

  if (shaders.size() == 0 || stages == 0) {
    debugPrint(DebugSeverity::Error, "No valid shaders for pipeline.");
    return;
  }

  // Make sure the correct shader stages are present.
  bool isCompute = (bool)(stages & VK_SHADER_STAGE_COMPUTE_BIT);
  if (isCompute && !(stages == VK_SHADER_STAGE_COMPUTE_BIT)) {
    debugPrint(DebugSeverity::Error, "Pipeline must be compute (only a compute shader) or graphics (only non-compute shaders).");
    return;
  }

  if (!isCompute && !(stages & VK_SHADER_STAGE_FRAGMENT_BIT)) {
    debugPrint(DebugSeverity::Error, "Graphics pipeline must include a fragment shader.");
    return;
  }

  if (!isCompute && !(stages & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT))) {
    debugPrint(DebugSeverity::Error, "Graphics pipeline must include a vertex or mesh shader.");
    return;
  }

  type_ = (isCompute) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

  // Merge descriptor set layout bindings.
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
  uint32_t highestBinding {0};
  // For each shader...
  for (auto& shader : shaders) {
    // Go through each layout binding in that shader...
    for (auto& shaderBinding : shader.layoutBindings) {
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
    return;
  }

  // Save descriptor binding types.
  descTypes_.resize(highestBinding+1);
  for (auto& binding : layoutBindings) {
    descTypes_[binding.binding] = binding.descriptorType;
  }

  // Merge push constants.
  for (auto& shader : shaders) {
    if (shader.pushConstants.stageFlags) {
      if (!pushConstRange_.stageFlags) {
        pushConstRange_ = VkPushConstantRange{
          .offset = shader.pushConstants.offset,
          .size = shader.pushConstants.size
        };
      }
      if (pushConstRange_.offset != shader.pushConstants.offset) {
        debugPrint(DebugSeverity::Error, "Shader push constant offset mismatch.");
        return;
      }
      if (pushConstRange_.size != shader.pushConstants.size) {
        debugPrint(DebugSeverity::Error, "Shader push constant size mismatch.");
        return;
      }
      pushConstRange_.stageFlags |= shader.pushConstants.stageFlags;
    }
  }

  // Use the descriptor set layout and push constant range to create the pipeline layout.
  VkPipelineLayoutCreateInfo plci {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &descLayout_,
    .pushConstantRangeCount = (uint32_t)((pushConstRange_.stageFlags == 0) ? 0 : 1),
    .pPushConstantRanges = ((pushConstRange_.stageFlags == 0) ? nullptr : &pushConstRange_) };
  if (!VKCHECK(vkCreatePipelineLayout(context.device_, &plci, nullptr, &layout_)) || !layout_) {
    debugPrint(DebugSeverity::Error, "Failed to create pipeline layout.");
    return;
  }

  // From here, we branch depending on whether we're creating a compute or graphics pipeline.
  if (isCompute) {
    VkComputePipelineCreateInfo pci {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaders.front().shader,
        .pName = shaders.front().entry
    },
      .layout = layout_ };
    if (!VKCHECK(vkCreateComputePipelines(context_.device_, nullptr, 1, &pci, nullptr, &pipeline_)) || !pipeline_) {
      debugPrint(DebugSeverity::Error, "Failed to create compute pipeline.");
      return;
    }
  }
  else {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.reserve(shaders.size());
    for (auto& shader : shaders) {
      shaderStages.push_back(VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = (VkShaderStageFlagBits)shader.stage,
        .module = shader.shader,
        .pName = shader.entry });
    }
    VkPipelineVertexInputStateCreateInfo vertexInput {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
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
      .stencilTestEnable = false };
    std::vector<VkPipelineColorBlendAttachmentState> colorAttachmentBlends;
    std::vector<VkFormat> colorAttachmentFormats;
    for (auto& attachment : params.colorAttachments) {
      colorAttachmentBlends.push_back(attachment.blend ?
        VkPipelineColorBlendAttachmentState{
          .blendEnable = true,
          .srcColorBlendFactor = translate(attachment.blend->srcColorFactor),
          .dstColorBlendFactor = translate(attachment.blend->dstColorFactor),
          .colorBlendOp = translate(attachment.blend->colorOp),
          .srcAlphaBlendFactor = translate(attachment.blend->srcAlphaFactor),
          .dstAlphaBlendFactor = translate(attachment.blend->dstAlphaFactor),
          .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT |  
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT } :
      VkPipelineColorBlendAttachmentState{
        .blendEnable = false,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |  
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT });
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
  }

  initSuccess_ = true;
}

wcgl::Pipeline::~Pipeline() {
  vkDeviceWaitIdle(context_.device_); // TODO: Queue destruction so we don't have to wait for an idle device.
  if (pipeline_) vkDestroyPipeline(context_.device_, pipeline_, nullptr);
  if (layout_) vkDestroyPipelineLayout(context_.device_, layout_, nullptr);
  if (descLayout_) vkDestroyDescriptorSetLayout(context_.device_, descLayout_, nullptr);
}