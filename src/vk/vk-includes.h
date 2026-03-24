#ifndef HLGL_VK_INCLUDES_H
#define HLGL_VK_INCLUDES_H

#define VK_NO_PROTOTYPES
#include <volk.h>
#ifdef _WIN32
#include <vma/vk_mem_alloc.h>
#else
#include <vk_mem_alloc.h>
#endif

#include <vulkan/vk_enum_string_helper.h>

#endif // HLGL_VK_INCLUDES_H