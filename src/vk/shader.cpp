#include "shader.h"
#include "context.h"

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

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
        { DEBUG_ERROR("Failed to compile shader source: %s", (const char*)diagnostic->getBufferPointer()); }
      else
        { DEBUG_ERROR("Failed to compile shader source."); }
      return;
    }
    slangModule->getTargetCode(0, spirv.writeRef(), diagnostic.writeRef());
    if (!spirv) {
      if (diagnostic)
        { DEBUG_ERROR("Failed to retrieve compiled shader: %s", (const char*)diagnostic->getBufferPointer()); }
      else
        { DEBUG_ERROR("Failed to retrieve compiled shader."); }
      return;
    }
    spvSrc = (uint32_t*)spirv->getBufferPointer();
    spvSize = spirv->getBufferSize();
  }

  if (!spvSrc || !spvSize) {
    DEBUG_ERROR("No shader source provided.");
    return;
  }

  // Create the Vulkan shader module.
  VkShaderModuleCreateInfo ci {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = spvSize,
    .pCode = spvSrc };
  if (!VKCHECK(vkCreateShaderModule(getDevice(), &ci, nullptr, &module)) || !module) {
    DEBUG_ERROR("Failed to create shader module.");
    return;
  }
}

hlgl::Shader::~Shader() {
  if (!_pimpl) return;
  if (_pimpl->module)
    vkDestroyShaderModule(getDevice(), _pimpl->module, nullptr);
}
