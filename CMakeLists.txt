cmake_minimum_required(VERSION 3.17)
project(tcl_imgui_sine_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-------------------------------------------------
#  Dependencies: OpenGL, GLFW, Tcl, SWIG, GLEW
#-------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(TCL 8.6 REQUIRED)
find_package(SWIG 4 REQUIRED)
find_package(GLEW REQUIRED)          # system GLEW (loader)
include(UseSWIG)

#-------------------------------------------------
#  cimgui submodule
#-------------------------------------------------
set(CIMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui)
add_subdirectory(${CIMGUI_DIR} external/cimgui_build)   # target: cimgui

#-------------------------------------------------
#  Dear ImGui sources inside cimgui (handle old layouts)
#-------------------------------------------------
set(IMGUI_DIR ${CIMGUI_DIR}/imgui)
if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    set(IMGUI_DIR ${CIMGUI_DIR}/third-party/imgui)
endif()
if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    set(IMGUI_DIR ${CIMGUI_DIR}/thirdparty/imgui)
endif()
if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    message(FATAL_ERROR "Could not locate imgui inside ${CIMGUI_DIR}")
endif()

#-------------------------------------------------
#  ImGui back-end (GLFW + GL3, GLEW loader)
#-------------------------------------------------
set(IMGUI_BACKEND_SRC
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

add_library(imgui_backend STATIC ${IMGUI_BACKEND_SRC})
target_include_directories(imgui_backend PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLEW_INCLUDE_DIRS})
target_compile_definitions(imgui_backend
    PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLEW)
target_link_libraries(imgui_backend
    PUBLIC glfw OpenGL::GL GLEW::GLEW)

#-------------------------------------------------
#  SWIG Tcl binding  (imgui_tcl)
#-------------------------------------------------
set(CMAKE_SWIG_FLAGS
    -I${CIMGUI_DIR}
    -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/imgui.i PROPERTIES CPLUSPLUS ON)

swig_add_library(imgui_tcl
    TYPE MODULE
    LANGUAGE tcl
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/imgui.i)

swig_link_libraries(imgui_tcl PRIVATE cimgui ${TCL_LIBRARIES} dl)
target_include_directories(imgui_tcl PRIVATE
    ${CIMGUI_DIR}
    ${TCL_INCLUDE_PATH})
target_compile_definitions(imgui_tcl PRIVATE
    CIMGUI_DEFINE_ENUMS_AND_STRUCTS)

#-------------------------------------------------
#  PortAudio (submodule)
#-------------------------------------------------
add_subdirectory(external/portaudio EXCLUDE_FROM_ALL)
set(PORTAUDIO_LIB portaudio)   # adjust if pa_static on your platform

#-------------------------------------------------
#  RtMidi (header-only + one .cpp)
#-------------------------------------------------
add_library(rtmidi STATIC external/rtmidi/RtMidi.cpp)
target_include_directories(rtmidi PUBLIC external/rtmidi)
if(UNIX AND NOT APPLE)
    target_link_libraries(rtmidi PUBLIC asound pthread)
endif()

#-------------------------------------------------
#  Main demo executable
#-------------------------------------------------
add_executable(sine_demo
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${IMGUI_BACKEND_SRC})   # optional: embed backend .cpp for auto-IDE symbols

target_include_directories(sine_demo PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${TCL_INCLUDE_PATH}
    ${GLEW_INCLUDE_DIRS})

target_compile_definitions(sine_demo PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLEW)

target_link_libraries(sine_demo PRIVATE
    imgui_backend
    cimgui
    glfw
    OpenGL::GL
    GLEW::GLEW
    ${TCL_LIBRARY}
    dl pthread
    ${PORTAUDIO_LIB}
    rtmidi)

#-------------------------------------------------
#  Post-build: copy Tcl script & module
#-------------------------------------------------
add_custom_command(TARGET sine_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sine_demo>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:imgui_tcl>
            $<TARGET_FILE_DIR:sine_demo>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/sine_ui.tcl
            $<TARGET_FILE_DIR:sine_demo>)

# Convenience target:  `make run`
add_custom_target(run
    COMMAND sine_demo
    DEPENDS sine_demo
    WORKING_DIRECTORY $<TARGET_FILE_DIR:sine_demo>)
