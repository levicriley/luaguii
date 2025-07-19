cmake_minimum_required(VERSION 3.17)
project(imgui_luajit_sol_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wshadow -Wpedantic)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3   3.3 REQUIRED)
find_package(GLEW    REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

# in‑tree builds
add_subdirectory(external/cimgui     external/cimgui_build)
add_subdirectory(external/portaudio  external/portaudio_build)
add_subdirectory(external/rtmidi     external/rtmidi_build)

# imgui headers
set(CIMGUI_DIR ${CMAKE_SOURCE_DIR}/external/cimgui)
if(EXISTS "${CIMGUI_DIR}/imgui/imgui.h")
  set(IMGUI_DIR "${CIMGUI_DIR}/imgui")
else()
  set(IMGUI_DIR "${CIMGUI_DIR}/third-party/imgui")
endif()

add_executable(sine_demo
  main.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(sine_demo PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${LUAJIT_INCLUDE_DIRS}
  $<TARGET_PROPERTY:rtmidi,INTERFACE_INCLUDE_DIRECTORIES>
  ${CMAKE_SOURCE_DIR}/external/sol2/include
)

target_compile_definitions(sine_demo PRIVATE
  IMGUI_IMPL_OPENGL_LOADER_GLEW
)

target_link_libraries(sine_demo PRIVATE
  cimgui
  portaudio
  rtmidi
  ${LUAJIT_LIBRARIES}
  glfw OpenGL::GL GLEW::GLEW
)

add_custom_command(TARGET sine_demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/gui.lua $<TARGET_FILE_DIR:sine_demo>
)
add_custom_target(run
  COMMAND sine_demo
  DEPENDS sine_demo
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sine_demo>
)
