#ifndef HLGL_VK_DEBUG_H
#define HLGL_VK_DEBUG_H

#include <hlgl.h>
#include "vulkan-includes.h"
#include <format>

namespace hlgl {
  
  void debugPrint(DebugSeverity severity, std::string_view message);
  
  inline bool checkVkResult(VkResult result, const char* expr, hlgl::DebugSeverity failSeverity, bool swapchain = false) {
    if (result == VK_SUCCESS || swapchain && (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR))
      return true;
    else {
      hlgl::debugPrint(failSeverity, std::format("'{}' failed; returned '{}'.", expr, string_VkResult(result)));
      return false;
    }
  }
}

// Check the VkResult returned by an expression, prints an error on failure.
#define VKCHECK(expr)      hlgl::checkVkResult(expr, #expr, hlgl::DebugSeverity::Error)

// Check the VkResult returned by an expression, prints a warning on failure.
#define VKCHECK_WARN(expr) hlgl::checkVkResult(expr, #expr, hlgl::DebugSeverity::Warning)

// Check the VkResult returned by an expression, prints an error on failure.
// Does not consider VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR to be errors.
#define VKCHECK_SWAPCHAIN(expr)      hlgl::checkVkResult(expr, #expr, hlgl::DebugSeverity::Error, true)


#endif // HLGL_VK_DEBUG_H