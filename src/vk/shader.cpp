#include "shader.h"
#include "context.h"
#include "debug.h"

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <spirv_reflect.h>

namespace {

  Slang::ComPtr<slang::IGlobalSession> slangGlobalSession_s {nullptr};

} // namespace <anon>

hlgl::Shader::Shader(Shader::CreateParams params)
: _pimpl(std::make_unique<ShaderImpl>(std::move(params)))
{ if (!_pimpl->module) _pimpl.reset(); }

hlgl::ShaderImpl::ShaderImpl(Shader::CreateParams&& params)
{
  if (!slangGlobalSession_s) {
    SlangGlobalSessionDesc desc {.enableGLSL = true};
    slang::createGlobalSession(&desc, slangGlobalSession_s.writeRef());
  }

  Slang::ComPtr<slang::IBlob> spirv;
  const uint32_t* spvSrc {nullptr};
  size_t spvSize {0};

  if (params.spvData) {
    spvSrc = (uint32_t*)params.spvData;
    spvSize = params.spvSize;
  }
  else {
    slang::TargetDesc slangTargets {.format = SLANG_SPIRV, .profile = slangGlobalSession_s->findProfile("spirv_1_4")};
    slang::CompilerOptionEntry slangOptions {.name = slang::CompilerOptionName::EmitSpirvDirectly, .value = {slang::CompilerOptionValueKind::Int, 1}};
    slang::SessionDesc slangSessionDesc {
      .targets = &slangTargets,
      .targetCount = 1,
      .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
      .compilerOptionEntries = &slangOptions,
      .compilerOptionEntryCount = 1};
    Slang::ComPtr<slang::ISession> slangSession;
    slangGlobalSession_s->createSession(slangSessionDesc, slangSession.writeRef());

    Slang::ComPtr<slang::IBlob> diagnostic;
    Slang::ComPtr<slang::IModule> slangModule { slangSession->loadModuleFromSourceString(params.debugName, nullptr, params.src, diagnostic.writeRef()) };
    if (!slangModule) {
      if (diagnostic)
        debugPrint(DebugSeverity::Error, std::format("Failed to compile shader source: {}", (const char*)diagnostic->getBufferPointer()));
      else
        debugPrint(DebugSeverity::Error, "Failed to compile shader source.");
      return;
    }
    slangModule->getTargetCode(0, spirv.writeRef(), diagnostic.writeRef());
    if (!spirv) {
      if (diagnostic)
        debugPrint(DebugSeverity::Error, std::format("Failed to retrieve compiled shader: {}", (const char*)diagnostic->getBufferPointer()));
      else
        debugPrint(DebugSeverity::Error, "Failed to retrieve compiled shader.");
      return;
    }
    spvSrc = (uint32_t*)spirv->getBufferPointer();
    spvSize = spirv->getBufferSize();
  }

  if (!spvSrc || !spvSize) {
    debugPrint(DebugSeverity::Error, "No shader source provided.");
    return;
  }

  // Create the Vulkan shader module.
  VkShaderModuleCreateInfo ci {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = spvSize,
    .pCode = spvSrc };
  if (!VKCHECK(vkCreateShaderModule(getDevice(), &ci, nullptr, &module)) || !module) {
    debugPrint(DebugSeverity::Error, "Failed to create shader module.");
    return;
  }

  // Create the Spirv-Reflect shader module.
  SpvReflectShaderModule spvModule;
  if (spvReflectCreateShaderModule(spvSize, spvSrc, &spvModule) != SPV_REFLECT_RESULT_SUCCESS) {
    debugPrint(DebugSeverity::Error, "Failed to create SpirV-Reflect shader module.");
    return;
  }

  stage = spvModule.shader_stage;

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
      .stageFlags = stage });
  }

  // Get the push constant range.
  if (spvModule.push_constant_block_count > 1)
    hlgl::debugPrint(hlgl::DebugSeverity::Warning, "Can't create a shader with more than one push constant block.");
  if (spvModule.push_constant_block_count > 0 && spvModule.push_constant_blocks) {
    pushConstants.stageFlags = stage;
    pushConstants.offset = spvModule.push_constant_blocks->offset;
    pushConstants.size = spvModule.push_constant_blocks->size;
  }

  spvReflectDestroyShaderModule(&spvModule);
}

hlgl::Shader::~Shader() {
  if (!_pimpl) return;
  if (_pimpl->module)
    vkDestroyShaderModule(getDevice(), _pimpl->module, nullptr);
}
