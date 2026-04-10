#ifndef HLGL_VK_INCLUDES_H
#define HLGL_VK_INCLUDES_H

#define VK_NO_PROTOTYPES
#include <volk.h>
#include <vk_mem_alloc.h>

#include <vulkan/vk_enum_string_helper.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#if defined HLGL_WINDOW_LIBRARY_GLFW
#include <backends/imgui_impl_glfw.h>
#elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
#include <backends/imgui_impl_win32.h>
#endif

#endif // HLGL_VK_INCLUDES_H