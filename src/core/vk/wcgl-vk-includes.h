#pragma once

#define VK_NO_PROTOTYPES
#include <volk.h>
#ifdef WIN32
#include <vma/vk_mem_alloc.h>
#else
#include <vk_mem_alloc.h>
#endif