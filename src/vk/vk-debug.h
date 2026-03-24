#ifndef HLGL_VK_DEBUG_H
#define HLGL_VK_DEBUG_H

#include "vk-includes.h"
#include "../utils/debug.h"
#include <format>

inline bool checkVkResult(VkResult result, const char* expr, hlgl::DebugSeverity failSeverity) {
  if (result == VK_SUCCESS)
    return true;
  else {
    hlgl::debugPrint(failSeverity, std::format("'{}' failed; returned '{}'.", expr, string_VkResult(result)));
    return false;
  }
}

// Check the VkResult returned by an expression, prints an error on failure.
#define VKCHECK(expr)      checkVkResult(expr, #expr, hlgl::DebugSeverity::Error)

// Check the VkResult returned by an expression, prints a warning on failure.
#define VKCHECK_WARN(expr) checkVkResult(expr, #expr, hlgl::DebugSeverity::Warning)


#endif // HLGL_VK_DEBUG_H