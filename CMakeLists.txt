cmake_minimum_required (VERSION 3.21)
project (hlgl VERSION 0.16 
         DESCRIPTION "A High Level Graphics Library designed for the Witchcraft game engine written by Haydn V. Harach"
         LANGUAGES C CXX)

## Define options. ##
set(HLGL_WINDOW_LIBRARIES GLFW SDL NATIVE_WIN32 NATIVE_XCB)
set(HLGL_WINDOW_LIBRARY GLFW CACHE STRING "The windowing library to use.")
set_property(CACHE HLGL_WINDOW_LIBRARY PROPERTY STRINGS ${HLGL_WINDOW_LIBRARIES})
if (NOT HLGL_WINDOW_LIBRARY IN_LIST HLGL_WINDOW_LIBRARIES)
  message(FATAL_ERROR "HLGL_WINDOW_LIBRARY must be one of ${HLGL_WINDOW_LIBRARIES}")
endif()

set(HLGL_GRAPHICS_APIS DIRECT3D METAL OPENGL VULKAN)
set(HLGL_GRAPHICS_API VULKAN CACHE STRING "The underlying graphics API to use.")
set_property(CACHE HLGL_GRAPHICS_API PROPERTY STRINGS &{HLGL_GRAPHICS_APIS})
if (NOT HLGL_GRAPHICS_API IN_LIST HLGL_GRAPHICS_APIS)
  message(FATAL_ERROR "HLGL_GRAPHICS_API must be one of ${HLGL_GRAPHICS_APIS}")
endif()

option(HLGL_BUILD_EXAMPLES "Build example applications that test and showcase HLGL functionality." ON)

option(HLGL_INCLUDE_IMGUI "Include and build the ImGui library for HLGL." ON)

## Define the hlgl library. ##
add_library(hlgl)
set_target_properties(hlgl PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED true
  CXX_EXTENSIONS false)

# Ignore "Missing PDB" warning (4099).
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set_target_properties(hlgl PROPERTIES LINK_FLAGS "-Wl,/ignore:4099")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set_target_properties(hlgl PROPERTIES LINK_FLAGS "/ignore:4099")
endif ()

target_include_directories(hlgl PUBLIC "include")
file (GLOB HLGL_CORE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp")
target_sources(hlgl PRIVATE ${HLGL_CORE_SOURCE_FILES})

add_subdirectory(thirdparty/glm EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/tinyobjloader EXCLUDE_FROM_ALL)

## If the chosen window library is GLFW... ##
if (HLGL_WINDOW_LIBRARY STREQUAL GLFW)
  set(GLFW_BUILD_WAYLAND 0)
  target_compile_definitions(hlgl PUBLIC -DHLGL_WINDOW_LIBRARY_GLFW)

  ## Add GLFW-specific libraries. ##
  add_subdirectory(thirdparty/glfw EXCLUDE_FROM_ALL)
  target_link_libraries(hlgl PRIVATE glfw)
## If the chosen window library i NATIVE_WIN32... ##
elseif (HLGL_WINDOW_LIBRARY STREQUAL NATIVE_WIN32)
  target_compile_definitions(hlgl PUBLIC -dHLGL_WINDOW_LIBRARY_NATIVE_WIN32)
else()
  message(FATAL_ERROR "${HLGL_WINDOW_LIBRARY} currently has no implementation.")
endif()

## If the chosen graphics API is Vulkan... ##
if (HLGL_GRAPHICS_API STREQUAL VULKAN)
  target_compile_definitions(hlgl PUBLIC -DHLGL_GRAPHICS_API_VULKAN)
  file(GLOB HLGL_BACKEND_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/vk/*.cpp")
  target_sources(hlgl PRIVATE ${HLGL_BACKEND_SOURCE_FILES})

  ## Add Vulkan-specific libraries. ##
  add_subdirectory(thirdparty/VulkanMemoryAllocator EXCLUDE_FROM_ALL)
  target_link_libraries(hlgl PRIVATE VulkanMemoryAllocator)

  add_subdirectory(thirdparty/volk EXCLUDE_FROM_ALL)
  target_link_libraries(hlgl PRIVATE volk)

  add_subdirectory(thirdparty/KTX-Software/lib ktx EXCLUDE_FROM_ALL)
  target_link_libraries(hlgl PRIVATE ktx)

  add_library(slang STATIC IMPORTED GLOBAL)
  if (WIN32)
    if (CMAKE_BUILD_TYPE STREQUAL Release)
      find_library(SLANG_LIB slang $ENV{VULKAN_SDK}/Lib)
    elseif (CMAKE_BUILD_TYPE STREQUAL Debug)
      find_library(SLANG_LIB slangd $ENV{VULKAN_SDK}/Lib)
    endif()
  elseif(UNIX)
    find_library(SLANG_LIB libslang.a)
  endif()
  if (SLANG_LIB-NOTFOUND)
    message(FATAL_ERROR "Could not find libslang")
  else()
    SET_PROPERTY(TARGET slang PROPERTY IMPORTED_LOCATION ${SLANG_LIB})
  endif()
  target_include_directories(slang INTERFACE include)

  # Ignore "Missing PDB" warning (4099).
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  #  set_target_properties(slang PROPERTIES LINK_FLAGS "-Wl,/ignore:4099")
    target_compile_options(VulkanMemoryAllocator INTERFACE -Wno-nullability-completeness)
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_target_properties(slang PROPERTIES LINK_FLAGS "/ignore:4099")
  endif ()

  target_link_libraries(hlgl PRIVATE slang)

  file (GLOB IMGUI_SOURCE_FILES "thirdparty/imgui/*.cpp" "thirdparty/imgui/backends/imgui_impl_vulkan.cpp")
  add_library(IMGUI STATIC ${IMGUI_SOURCE_FILES})
  if (HLGL_WINDOW_LIBRARY STREQUAL GLFW)
    target_sources(IMGUI PRIVATE "thirdparty/imgui/backends/imgui_impl_glfw.cpp")
    target_link_libraries(IMGUI PRIVATE glfw)
  elseif (HLGL_WINDOW_LIBRARY STREQUAL NATIVE_WIN32)
    target_sources(IMGUI PRIVATE "thirdparty/imgui/backends/imgui_impl_win32.cpp")
  endif()
  target_compile_definitions(IMGUI PUBLIC -DIMGUI_IMPL_VULKAN_USE_VOLK)
  target_include_directories(IMGUI PUBLIC thirdparty/imgui)
  target_link_libraries(IMGUI PRIVATE volk)
  target_link_libraries(hlgl PRIVATE IMGUI)

else()
  message(FATAL_ERROR "${HLGL_GRAPHICS_API} currently has no implementation.")
endif()

## Add backend source files to the hlgl library. ##
target_sources(hlgl PRIVATE ${HLGL_BACKEND_SOURCE_FILES})

## Create example applications. ##
if (HLGL_BUILD_EXAMPLES)

  ## Get each .cpp file in src/examples/.  Each one is an example executable. ##
  file (GLOB_RECURSE EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/examples/*.cpp")
  foreach (EXAMPLE_SOURCE ${EXAMPLE_SOURCES})

    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED true
      CXX_EXTENSIONS false)

    # Ignore "Missing PDB" warning (4099).
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      set_target_properties(${EXAMPLE_NAME} PROPERTIES LINK_FLAGS "-Wl,/ignore:4099")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      set_target_properties(${EXAMPLE_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
    endif ()

    target_link_libraries(${EXAMPLE_NAME} PRIVATE hlgl glfw glm IMGUI tinyobjloader)
    install(TARGETS ${EXAMPLE_NAME})

  endforeach (EXAMPLE_SOURCE)

endif()