cmake_minimum_required (VERSION 3.21)
project (wcgl VERSION 0.16 
         DESCRIPTION "A Graphics Library designed for the Witchcraft game engine written by Haydn V. Harach"
         LANGUAGES C CXX)

## Define options. ##
set(WCGL_WINDOW_LIBRARIES GLFW SDL NATIVE_WIN32 NATIVE_XCB)
set(WCGL_WINDOW_LIBRARY GLFW CACHE STRING "The windowing library to use.")
set_property(CACHE WCGL_WINDOW_LIBRARY PROPERTY STRINGS ${WCGL_WINDOW_LIBRARIES})
if (NOT WCGL_WINDOW_LIBRARY IN_LIST WCGL_WINDOW_LIBRARIES)
  message(FATAL_ERROR "WCGL_WINDOW_LIBRARY must be one of ${WCGL_WINDOW_LIBRARIES}")
endif()

set(WCGL_GRAPHICS_APIS DIRECT3D METAL OPENGL VULKAN)
set(WCGL_GRAPHICS_API VULKAN CACHE STRING "The underlying graphics API to use.")
set_property(CACHE WCGL_GRAPHICS_API PROPERTY STRINGS &{WCGL_GRAPHICS_APIS})
if (NOT WCGL_GRAPHICS_API IN_LIST WCGL_GRAPHICS_APIS)
  message(FATAL_ERROR "WCGL_GRAPHICS_API must be one of ${WCGL_GRAPHICS_APIS}")
endif()

option(WCGL_BUILD_EXAMPLES "Build example applications that test and showcase WCGL functionality." ON)

option(WCGL_INCLUDE_IMGUI "Include and build the ImGui library for WCGL." ON)

## Define the library. ##
add_library(wcgl)
set_target_properties(wcgl PROPERTIES
  LINK_FLAGS "/ignore:4099"
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED true
  CXX_EXTENSIONS false)
target_include_directories(wcgl PUBLIC "include")
file (GLOB WCGL_CORE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp")
target_sources(wcgl PRIVATE ${WCGL_CORE_SOURCE_FILES})


## Add universal libraries. ##
add_subdirectory(thirdparty/fmt EXCLUDE_FROM_ALL)
target_link_libraries(wcgl PRIVATE fmt)

add_subdirectory(thirdparty/glm EXCLUDE_FROM_ALL)

## If the chosen window library is GLFW... ##
if (WCGL_WINDOW_LIBRARY STREQUAL GLFW)
  target_compile_definitions(wcgl PUBLIC -DWCGL_WINDOW_LIBRARY_GLFW)

  ## Add GLFW-specific libraries. ##
  add_subdirectory(thirdparty/glfw EXCLUDE_FROM_ALL)
  target_link_libraries(wcgl PRIVATE glfw)

else()
  message(FATAL_ERROR "${WCGL_WINDOW_LIBRARY} currently has no implementation.")
endif()

## If the chosen graphics API is Vulkan... ##
if (WCGL_GRAPHICS_API STREQUAL VULKAN)
  target_compile_definitions(wcgl PUBLIC -DWCGL_GRAPHICS_API_VULKAN)
  file(GLOB WCGL_BACKEND_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/vk/*.cpp")
  target_sources(wcgl PRIVATE ${WCGL_BACKEND_SOURCE_FILES})

  ## Add Vulkan-specific libraries. ##
  find_package(Vulkan REQUIRED)
  target_link_libraries(wcgl PRIVATE Vulkan::Vulkan)

  add_subdirectory(thirdparty/volk EXCLUDE_FROM_ALL)
  target_link_libraries(wcgl PRIVATE volk)

  add_library(shaderc STATIC IMPORTED GLOBAL)
  if (WIN32)
    if (CMAKE_BUILD_TYPE STREQUAL Release)
      find_library(SHADERC_LIB shaderc_combined $ENV{VULKAN_SDK}/Lib)
    elseif (CMAKE_BUILD_TYPE STREQUAL Debug)
      find_library(SHADERC_LIB shaderc_combinedd $ENV{VULKAN_SDK}/Lib)
    endif()
  elseif(UNIX)
    find_library(SHADERC_LIB libshaderc_combined.a)
  endif()
  if (SHADERC_LIB-NOTFOUND)
    message(FATAL_ERROR "Could not find libshaderc_combined")
  else()
    SET_PROPERTY(TARGET shaderc PROPERTY IMPORTED_LOCATION ${SHADERC_LIB})
  endif()
  target_include_directories(shaderc INTERFACE include)
  set_target_properties(shaderc PROPERTIES LINK_FLAGS "/ignore:4099")
  target_link_libraries(wcgl PRIVATE shaderc)

  set (SPIRV_REFLECT_EXECUTABLE OFF CACHE INTERNAL "" FORCE)
  set (SPIRV_REFLECT_EAMPLES OFF CACHE INTERNAL "" FORCE)
  set (SPIRV_REFLECT_STATIC_LIB ON CACHE INTERNAL "" FORCE)
  add_subdirectory(thirdparty/SPIRV-Reflect EXCLUDE_FROM_ALL)
  target_link_libraries(wcgl PRIVATE spirv-reflect-static)

  if (WCGL_INCLUDE_IMGUI)
    target_compile_definitions(wcgl PUBLIC -DWCGL_INCLUDE_IMGUI)
    file (GLOB IMGUI_SOURCE_FILES "thirdparty/imgui/*.cpp" "thirdparty/imgui/backends/imgui_impl_glfw.cpp" "thirdparty/imgui/backends/imgui_impl_vulkan.cpp")
    add_library(IMGUI STATIC ${IMGUI_SOURCE_FILES})
    target_compile_definitions(IMGUI PUBLIC -DIMGUI_IMPL_VULKAN_USE_VOLK)
    target_include_directories(IMGUI PUBLIC thirdparty/imgui)
    target_link_libraries(IMGUI PRIVATE glfw Vulkan::Vulkan volk)
    target_link_libraries(wcgl PRIVATE IMGUI)
  endif ()

else()
  message(FATAL_ERROR "${WCGL_GRAPHICS_API} currently has no implementation.")
endif()

## Add source files to the wcgl library. ##
target_sources(wcgl PRIVATE ${WCGL_BACKEND_SOURCE_FILES})

## Create example applications. ##
if (WCGL_BUILD_EXAMPLES)

  ## Get each .cpp file in src/examples/.  Each one is an example executable. ##
  file (GLOB_RECURSE EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/examples/*.cpp")
  foreach (EXAMPLE_SOURCE ${EXAMPLE_SOURCES})

    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
      LINK_FLAGS "/ignore:4099"
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED true
      CXX_EXTENSIONS false)
    target_link_libraries(${EXAMPLE_NAME} PRIVATE wcgl fmt glfw glm IMGUI)
    install(TARGETS ${EXAMPLE_NAME})

  endforeach (EXAMPLE_SOURCE)

endif()