#pragma once

#include "wcgl-vk-includes.h"
#include "../wcgl-debug.h"
#include <fmt/format.h>

// Custom fmt::formatter for VkResult.
template <> struct fmt::formatter<VkResult> : formatter<string_view> {
  auto format(VkResult result, format_context& ctx) const {
    string_view str {""};
    switch (result) {
    case VK_SUCCESS: str = "VK_SUCCESS"; break;
    case VK_NOT_READY: str = "VK_NOT_READY"; break;
    case VK_TIMEOUT: str = "VK_TIMEOUT"; break;
    case VK_EVENT_SET: str = "VK_EVENT_SET"; break;
    case VK_EVENT_RESET: str = "VK_EVENT_RESET"; break;
    case VK_INCOMPLETE: str = "VK_INCOMPLETE"; break;
    case VK_ERROR_OUT_OF_HOST_MEMORY: str = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: str = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
    case VK_ERROR_INITIALIZATION_FAILED: str = "VK_ERROR_INITIALIZATION_FAILED"; break;
    case VK_ERROR_DEVICE_LOST: str = "VK_ERROR_DEVICE_LOST"; break;
    case VK_ERROR_MEMORY_MAP_FAILED: str = "VK_ERROR_MEMORY_MAP_FAILED"; break;
    case VK_ERROR_LAYER_NOT_PRESENT: str = "VK_ERROR_LAYER_NOT_PRESENT"; break;
    case VK_ERROR_EXTENSION_NOT_PRESENT: str = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
    case VK_ERROR_FEATURE_NOT_PRESENT: str = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
    case VK_ERROR_INCOMPATIBLE_DRIVER: str = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
    case VK_ERROR_TOO_MANY_OBJECTS: str = "VK_ERROR_TOO_MANY_OBJECTS"; break;
    case VK_ERROR_FORMAT_NOT_SUPPORTED: str = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
    case VK_ERROR_FRAGMENTED_POOL: str = "VK_ERROR_FRAGMENTED_POOL"; break;
    case VK_ERROR_OUT_OF_POOL_MEMORY: str = "VK_ERROR_OUT_OF_POOL_MEMORY"; break;
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: str = "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break;
    case VK_ERROR_FRAGMENTATION: str = "VK_ERROR_FRAGMENTATION"; break;
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: str = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"; break;
    case VK_PIPELINE_COMPILE_REQUIRED: str = "VK_PIPELINE_COMPILE_REQUIRED"; break;
    case VK_ERROR_SURFACE_LOST_KHR: str = "VK_ERROR_SURFACE_LOST_KHR"; break;
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: str = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
    case VK_SUBOPTIMAL_KHR: str = "VK_SUBOPTIMAL_KHR"; break;
    case VK_ERROR_OUT_OF_DATE_KHR: str = "VK_ERROR_OUT_OF_DATE_KHR"; break;
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: str = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
    case VK_ERROR_VALIDATION_FAILED_EXT: str = "VK_ERROR_VALIDATION_FAILED_EXT"; break;
    case VK_ERROR_INVALID_SHADER_NV: str = "VK_ERROR_INVALID_SHADER_NV"; break;
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: str = "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR"; break;
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: str = "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR"; break;
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: str = "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR"; break;
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: str = "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR"; break;
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: str = "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR"; break;
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: str = "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED"; break;
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: str = "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; break;
    case VK_ERROR_NOT_PERMITTED_KHR: str = "VK_ERROR_NOT_PERMITTED_KHR"; break;
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: str = "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"; break;
    case VK_THREAD_IDLE_KHR: str = "VK_THREAD_IDLE_KHR"; break;
    case VK_THREAD_DONE_KHR: str = "VK_THREAD_DONE_KHR"; break;
    case VK_OPERATION_DEFERRED_KHR: str = "VK_OPERATION_DEFERRED_KHR"; break;
    case VK_OPERATION_NOT_DEFERRED_KHR: str = "VK_OPERATION_NOT_DEFERRED_KHR"; break;
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: str = "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR"; break;
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: str = "VK_ERROR_COMPRESSION_EXHAUSTED_EXT"; break;
    case VK_INCOMPATIBLE_SHADER_BINARY_EXT: str = "VK_INCOMPATIBLE_SHADER_BINARY_EXT"; break;
  //case VK_PIPELINE_BINARY_MISSING_KHR: str = "VK_PIPELINE_BINARY_MISSING_KHR"; break;
  //case VK_ERROR_NOT_ENOUGH_SPACE_KHR: str = "VK_ERROR_NOT_ENOUGH_SPACE_KHR"; break;
    default: str = "VK_ERROR_UNKNOWN"; break;
    }
    return formatter<string_view>::format(str, ctx);
  }
};

// Custom fmt::formatter for wcgl::GpuType.
template <> class fmt::formatter<wcgl::GpuType>: public formatter<string_view> {
public:
  auto format(wcgl::GpuType type, format_context& ctx) const {
    string_view str {""};
    switch (type) {
      case wcgl::GpuType::Cpu:        str = "Cpu"; break;
      case wcgl::GpuType::Virtual:    str = "Virtual"; break;
      case wcgl::GpuType::Integrated: str = "Integrated"; break;
      case wcgl::GpuType::Discrete:   str = "Discrete"; break;
      default: str = "Other"; break;
    }
    return formatter<string_view>::format(str, ctx);
  }
};

#define VKCHECK(expr) [&]{\
  VkResult result {expr}; \
  if (result == VK_SUCCESS) { return true; } \
  else { wcgl::debugPrint(wcgl::DebugSeverity::Error, fmt::format("'{}' failed; returned '{}'.", #expr, result)); return false; } \
}()