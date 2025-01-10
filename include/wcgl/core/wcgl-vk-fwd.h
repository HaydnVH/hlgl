#pragma once

// This file forward declares types from vulkan.h and vk_mem_alloc.h
// so WCGL types can contain their implementation state directly without
// using pImpl or pulling in these headers to the public interface.
// Not everything from vulkan_core.h needs to be present here,
// only the types which have to be defined in order to be part of HLGL core types.

#include <cstdint>

#ifndef VULKAN_H_

struct VkApplicationInfo;
struct VkBuffer_T;                  typedef VkBuffer_T* VkBuffer;
struct VkCommandBuffer_T;           typedef VkCommandBuffer_T* VkCommandBuffer;
struct VkCommandPool_T;             typedef VkCommandPool_T* VkCommandPool;
struct VkDebugUtilsMessengerEXT_T;  typedef VkDebugUtilsMessengerEXT_T* VkDebugUtilsMessengerEXT;
struct VkDescriptorPool_T;          typedef VkDescriptorPool_T* VkDescriptorPool;
struct VkDescriptorSetLayout_T;     typedef VkDescriptorSetLayout_T* VkDescriptorSetLayout;
struct VkDevice_T;                  typedef VkDevice_T* VkDevice;
struct VkDeviceMemory_T;            typedef VkDeviceMemory_T* VkDeviceMemory;
struct VkFence_T;                   typedef VkFence_T* VkFence;
struct VkImage_T;                   typedef VkImage_T* VkImage;
struct VkImageView_T;               typedef VkImageView_T* VkImageView;
struct VkInstance_T;                typedef VkInstance_T* VkInstance;
struct VkPhysicalDevice_T;          typedef VkPhysicalDevice_T* VkPhysicalDevice;
struct VkPipeline_T;                typedef VkPipeline_T* VkPipeline;
struct VkPipelineLayout_T;          typedef VkPipelineLayout_T* VkPipelineLayout;
struct VkQueue_T;                   typedef VkQueue_T* VkQueue;
struct VkSampler_T;                 typedef VkSampler_T* VkSampler;
struct VkSemaphore_T;               typedef VkSemaphore_T* VkSemaphore;
struct VkSurfaceKHR_T;              typedef VkSurfaceKHR_T* VkSurfaceKHR;
struct VkSwapchainKHR_T;            typedef VkSwapchainKHR_T* VkSwapchainKHR;

typedef uint32_t VkBool32;
typedef uint64_t VkDeviceAddress;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkFlags;
typedef uint32_t VkSampleMask;

typedef VkFlags VkAccessFlags;
typedef VkFlags VkPipelineStageFlags;
typedef VkFlags VkShaderStageFlags;

//enum VkDescriptorType;
enum VkFormat;
enum VkPipelineBindPoint;

// Has to be fully defined because pipeline contains a vector of these.
typedef enum VkDescriptorType {
  VK_DESCRIPTOR_TYPE_SAMPLER = 0,
  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
  VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
  VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
  VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10,
  VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK = 1000138000,
  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR = 1000150000,
  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV = 1000165000,
  VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM = 1000440000,
  VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM = 1000440001,
  VK_DESCRIPTOR_TYPE_MUTABLE_EXT = 1000351000,
  VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
  VK_DESCRIPTOR_TYPE_MUTABLE_VALVE = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
  VK_DESCRIPTOR_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkDescriptorType;

typedef enum VkImageLayout {
  VK_IMAGE_LAYOUT_UNDEFINED = 0,
  VK_IMAGE_LAYOUT_GENERAL = 1,
  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
  VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
  VK_IMAGE_LAYOUT_PREINITIALIZED = 8,
  VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL = 1000241000,
  VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL = 1000241001,
  VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL = 1000241002,
  VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL = 1000241003,
  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL = 1000314000,
  VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL = 1000314001,
  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002,
  VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR = 1000024000,
  VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR = 1000024001,
  VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR = 1000024002,
  VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR = 1000111000,
  VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT = 1000218000,
  VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR = 1000164003,
  VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR = 1000232000,
  VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR = 1000299000,
  VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR = 1000299001,
  VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR = 1000299002,
  VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT = 1000339000,
  VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
  VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
  VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
  VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
  VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
  VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
  VK_IMAGE_LAYOUT_MAX_ENUM = 0x7FFFFFFF
} VkImageLayout;

struct VkExtent2D { uint32_t width; uint32_t height; };
struct VkExtent3D { uint32_t width; uint32_t height; uint32_t depth; };
struct VkOffset2D { int32_t x; int32_t y; };
struct VkOffset3D { int32_t x; int32_t y; int32_t z; };
struct VkRect2D { VkOffset2D offset; VkExtent2D extent; };
struct VkPushConstantRange { VkShaderStageFlags stageFlags; uint32_t offset; uint32_t size; };

#endif // VULKAN_H_


#ifndef AMD_VULKAN_MEMORY_ALLOCATOR_H

struct VmaAllocation_T;  typedef VmaAllocation_T* VmaAllocation;
struct VmaAllocator_T;   typedef VmaAllocator_T* VmaAllocator;

struct VmaAllocationInfo {
  uint32_t memoryType;
  VkDeviceMemory deviceMemory;
  VkDeviceSize offset;
  VkDeviceSize size;
  void* pMappedData;
  void* pUserData;
  const char* pName;
};

#endif // AMD_VULKAN_MEMORY_ALLOCATOR_H