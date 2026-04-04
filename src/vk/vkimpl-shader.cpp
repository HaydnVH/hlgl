#include "vkimpl-includes.h"
#include "vkimpl-debug.h"
#include "vkimpl-translate.h"
#include "vkimpl-context.h"
#include <hlgl/context.h>

#include "vkimpl-shader.h"

#include <shaderc/shaderc.hpp>
#include <spirv_reflect.h>

hlgl::Shader::Shader(hlgl::ShaderParams params)
{
  std::vector<uint32_t> spvCompiled;
  const void* spvSrc {nullptr};
  size_t spvSize {0};

  // If the user provides a pointer and size for Spir-V, simply use that.
  if (params.spvData && params.spvSize) {
    spvSrc = params.spvData;
    spvSize = params.spvSize;
  }
  // If GLSL source code is provided, use shaderc to compile it to Spir-V.
  else if (params.src) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    shaderc_shader_kind kind {shaderc_glsl_infer_from_source};
    if (params.debugName) {
      std::string_view nameView{params.debugName};
      if (nameView.find(".vert") != std::string_view::npos) kind = shaderc_glsl_vertex_shader;
      else if (nameView.find(".frag") != std::string_view::npos) kind = shaderc_glsl_fragment_shader;
      else if (nameView.find(".geom") != std::string_view::npos) kind = shaderc_glsl_geometry_shader;
      else if (nameView.find(".tesc") != std::string_view::npos) kind = shaderc_glsl_tess_control_shader;
      else if (nameView.find(".tese") != std::string_view::npos) kind = shaderc_glsl_tess_evaluation_shader;
      else if (nameView.find(".comp") != std::string_view::npos) kind = shaderc_glsl_compute_shader;
      else if (nameView.find(".task") != std::string_view::npos) kind = shaderc_glsl_task_shader;
      else if (nameView.find(".mesh") != std::string_view::npos) kind = shaderc_glsl_mesh_shader;
    }

    std::string_view glsl {params.src};
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(glsl.data(), glsl.size(), kind, params.debugName, params.entry, options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
      debugPrint(DebugSeverity::Error, std::format("Failed to compile shader: {}", result.GetErrorMessage()));
      return;
    }
    spvCompiled = {result.cbegin(), result.cend()};
    spvSrc = (const void*)spvCompiled.data();
    spvSize = spvCompiled.size() * sizeof(uint32_t);
  }

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
  if (!VKCHECK(vkCreateShaderModule(_impl::getDevice(), &ci, nullptr, &shader_)) || !shader_) {
    debugPrint(DebugSeverity::Error, "Failed to create shader module.");
    return;
  }
  stage_ = spvModule.shader_stage;
  entry_ = params.entry;

  // Get the push constant range.
  if (spvModule.push_constant_block_count > 1)
    hlgl::debugPrint(hlgl::DebugSeverity::Warning, "Can't create a shader with more than one push constant block.");
  if (spvModule.push_constant_block_count > 0 && spvModule.push_constant_blocks) {
    pushConstants_.stageFlags = stage_;
    pushConstants_.offset = spvModule.push_constant_blocks->offset;
    pushConstants_.size = spvModule.push_constant_blocks->size;
  }

  // Get descriptor set layout bindings.
  uint32_t spvBindingCount {0};
  if (spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, nullptr) != SPV_REFLECT_RESULT_SUCCESS) return;
  std::vector<SpvReflectDescriptorBinding*> spvBindings(spvBindingCount);
  if (spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, spvBindings.data()) != SPV_REFLECT_RESULT_SUCCESS) return;

  layoutBindings_.reserve(spvBindingCount);
  for (uint32_t i {0}; i < spvBindingCount; ++i) {
    layoutBindings_.push_back(VkDescriptorSetLayoutBinding{
      .binding = spvBindings[i]->binding,
      .descriptorType = (VkDescriptorType)spvBindings[i]->descriptor_type,
      .descriptorCount = 1,
      .stageFlags = stage_
      });
  }

  spvReflectDestroyShaderModule(&spvModule);
  initSuccess_ = true;
}

hlgl::Shader::~Shader() {
  if (shader_)
    vkDestroyShaderModule(_impl::getDevice(), shader_, nullptr);
}