cmake_minimum_required (VERSION 3.21)
project (hlgl VERSION 0.16 
         DESCRIPTION "A Graphics Library designed for the Witchcraft game engine written by Haydn V. Harach"
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

## Define the hlgl-core library. ##
add_library(hlgl-core)
set_target_properties(hlgl-core PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED true
  CXX_EXTENSIONS false)
if (MSVC)
  set_target_properties(hlgl-core PROPERTIES LINK_FLAGS "/ignore:4099")
endif ()
target_include_directories(hlgl-core PUBLIC "include")
file (GLOB HLGL_CORE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp")
target_sources(hlgl-core PRIVATE ${HLGL_CORE_SOURCE_FILES})


## Add universal libraries. ##
add_subdirectory(thirdparty/fmt EXCLUDE_FROM_ALL)
target_link_libraries(hlgl-core PRIVATE fmt)

add_subdirectory(thirdparty/glm EXCLUDE_FROM_ALL)

## If the chosen window library is GLFW... ##
if (HLGL_WINDOW_LIBRARY STREQUAL GLFW)
  set(GLFW_BUILD_WAYLAND 0)
  target_compile_definitions(hlgl-core PUBLIC -DHLGL_WINDOW_LIBRARY_GLFW)

  ## Add GLFW-specific libraries. ##
  add_subdirectory(thirdparty/glfw EXCLUDE_FROM_ALL)
  target_link_libraries(hlgl-core PRIVATE glfw)

else()
  message(FATAL_ERROR "${HLGL_WINDOW_LIBRARY} currently has no implementation.")
endif()

## If the chosen graphics API is Vulkan... ##
if (HLGL_GRAPHICS_API STREQUAL VULKAN)
  target_compile_definitions(hlgl-core PUBLIC -DHLGL_GRAPHICS_API_VULKAN)
  file(GLOB HLGL_BACKEND_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/vk/*.cpp")
  target_sources(hlgl-core PRIVATE ${HLGL_BACKEND_SOURCE_FILES})

  ## Add Vulkan-specific libraries. ##
  find_package(Vulkan REQUIRED)
  target_link_libraries(hlgl-core PRIVATE Vulkan::Vulkan)

  add_subdirectory(thirdparty/volk EXCLUDE_FROM_ALL)
  target_link_libraries(hlgl-core PRIVATE volk)

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
  target_link_libraries(hlgl-core PRIVATE shaderc)

  set (SPIRV_REFLECT_EXECUTABLE OFF CACHE INTERNAL "" FORCE)
  set (SPIRV_REFLECT_EAMPLES OFF CACHE INTERNAL "" FORCE)
  set (SPIRV_REFLECT_STATIC_LIB ON CACHE INTERNAL "" FORCE)
  add_subdirectory(thirdparty/SPIRV-Reflect EXCLUDE_FROM_ALL)
  target_link_libraries(hlgl-core PRIVATE spirv-reflect-static)

  if (HLGL_INCLUDE_IMGUI)
    target_compile_definitions(hlgl-core PUBLIC -DHLGL_INCLUDE_IMGUI)
    file (GLOB IMGUI_SOURCE_FILES "thirdparty/imgui/*.cpp" "thirdparty/imgui/backends/imgui_impl_glfw.cpp" "thirdparty/imgui/backends/imgui_impl_vulkan.cpp")
    add_library(IMGUI STATIC ${IMGUI_SOURCE_FILES})
    target_compile_definitions(IMGUI PUBLIC -DIMGUI_IMPL_VULKAN_USE_VOLK)
    target_include_directories(IMGUI PUBLIC thirdparty/imgui)
    target_link_libraries(IMGUI PRIVATE glfw Vulkan::Vulkan volk)
    target_link_libraries(hlgl-core PRIVATE IMGUI)
  endif ()

else()
  message(FATAL_ERROR "${HLGL_GRAPHICS_API} currently has no implementation.")
endif()

## Add backend source files to the hlgl-core library. ##
target_sources(hlgl-core PRIVATE ${HLGL_BACKEND_SOURCE_FILES})

## Define the hlgl-plus library. ##
add_library(hlgl-plus)
set_target_properties(hlgl-plus PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED true
  CXX_EXTENSIONS false)
if (MSVC)
  set_target_properties(hlgl-plus PROPERTIES LINK_FLAGS "/ignore:4099")
endif ()
target_include_directories(hlgl-plus PUBLIC "include")
file (GLOB HLGL_PLUS_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/plus/*.cpp")
target_sources(hlgl-plus PRIVATE ${HLGL_PLUS_SOURCE_FILES})
target_link_libraries(hlgl-plus PUBLIC hlgl-core)
target_link_libraries(hlgl-plus PRIVATE fmt glm)

## Add the fastgltf library to hlgl-plus. ##
add_subdirectory(thirdparty/fastgltf EXCLUDE_FROM_ALL)
target_link_libraries(hlgl-plus PRIVATE fastgltf)

## Add the stb headers (mainly we care about stb_image.h) to hlgl-plus. ##
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE thirdparty/stb)
target_link_libraries(hlgl-plus PRIVATE stb)

## Create example applications. ##
if (HLGL_BUILD_EXAMPLES)

  ## Get each .cpp file in src/examples/.  Each one is an example executable. ##
  file (GLOB_RECURSE EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/examples/*.cpp")
  foreach (EXAMPLE_SOURCE ${EXAMPLE_SOURCES})

    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED true
      CXX_EXTENSIONS false)
    if (MSVC)
      set_target_properties(${EXAMPLE_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
    endif ()
    target_link_libraries(${EXAMPLE_NAME} PRIVATE hlgl-plus fmt glfw glm IMGUI)
    install(TARGETS ${EXAMPLE_NAME})

  endforeach (EXAMPLE_SOURCE)

endif()