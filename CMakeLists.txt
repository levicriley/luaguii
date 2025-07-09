cmake_minimum_required(VERSION 3.17)
project(tcl_imgui_sine_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-------------------------------------------------
# Dependencies: OpenGL, GLFW, Tcl, SWIG
#-------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(TCL 8.6 REQUIRED)
find_package(SWIG 4 REQUIRED)
include(UseSWIG)

#-------------------------------------------------
# cimgui submodule
#-------------------------------------------------
set(CIMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui)
add_subdirectory(${CIMGUI_DIR} external/cimgui_build)   # target: cimgui


# -------------------------------------------------------------------
# Locate Dear ImGui inside cimgui (handles old & new layouts)
# -------------------------------------------------------------------
set(IMGUI_DIR ${CIMGUI_DIR}/imgui)                      # ← new layout

if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    set(IMGUI_DIR ${CIMGUI_DIR}/third-party/imgui)      # layout 2023-04
endif()

if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    set(IMGUI_DIR ${CIMGUI_DIR}/thirdparty/imgui)       # very old layout
endif()

if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    message(FATAL_ERROR "Could not locate imgui inside ${CIMGUI_DIR}")
endif()


# -------------------------------------------------
# Build ImGui back-ends (GLFW + OpenGL3, loader = GLEW)
# -------------------------------------------------
find_package(GLEW REQUIRED)                       # <— system loader

set(IMGUI_BACKEND_SRC
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

#-------------------------
# SWIG binding: imgui_tcl
#-------------------------
set(CMAKE_SWIG_FLAGS
    -I${CIMGUI_DIR}
    -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/imgui.i PROPERTIES CPLUSPLUS ON)

swig_add_library(imgui_tcl
    TYPE MODULE
    LANGUAGE tcl
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/imgui.i
)

swig_link_libraries(imgui_tcl
    PRIVATE cimgui ${TCL_LIBRARIES} dl
)

# now also define the macro for the generated wrapper compile
target_compile_definitions(imgui_tcl
    PRIVATE
        CIMGUI_DEFINE_ENUMS_AND_STRUCTS
)

target_include_directories(imgui_tcl
    PRIVATE
        ${CIMGUI_DIR}        # for cimgui.h
        ${TCL_INCLUDE_PATH}  # for tcl.h
)

#-------------------------------------------------
# Main demo executable
#-------------------------------------------------
add_executable(sine_demo
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
     ${IMGUI_BACKEND_SRC}
)

target_include_directories(sine_demo
    PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${TCL_INCLUDE_PATH}
        ${GLEW_INCLUDE_DIRS})

# ← Use the same loader you picked for the back-end
target_compile_definitions(sine_demo PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLEW)

target_link_libraries(sine_demo
    PRIVATE
        cimgui
        glfw
        OpenGL::GL
        GLEW::GLEW         # if you’re using GLEW too
        ${TCL_LIBRARY}     # ← link the Tcl interpreter
        dl
        pthread)

# Copy runtime deps (.tcl script and swig lib) after build
add_custom_command(TARGET sine_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sine_demo>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:imgui_tcl> $<TARGET_FILE_DIR:sine_demo>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/sine_ui.tcl
            $<TARGET_FILE_DIR:sine_demo>)

# Convenience 'run' target
add_custom_target(run
    COMMAND sine_demo
    DEPENDS sine_demo
    WORKING_DIRECTORY $<TARGET_FILE_DIR:sine_demo>)
