cmake_minimum_required(VERSION 3.17)
project(lua_imgui_sine_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-------------------------------------------------
#  Dependencies: OpenGL, GLFW, LuaJIT, GLEW
#-------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

#-------------------------------------------------
#  Dear ImGui (regular, vendored)
#-------------------------------------------------
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)

set(IMGUI_SRC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp       # optional, remove if you don't want it
)

add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)

# Backends (GLFW + OpenGL3)
set(IMGUI_BACKEND_SRC
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_backend STATIC ${IMGUI_BACKEND_SRC})
target_include_directories(imgui_backend PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLEW_INCLUDE_DIRS}
)
target_compile_definitions(imgui_backend PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLEW)
target_link_libraries(imgui_backend PUBLIC glfw OpenGL::GL GLEW::GLEW imgui)

#-------------------------------------------------
#  PortAudio (submodule)
#-------------------------------------------------
add_subdirectory(external/portaudio EXCLUDE_FROM_ALL)
set(PORTAUDIO_LIB portaudio)  # change to pa_static if you built static

#-------------------------------------------------
#  RtMidi (single .cpp)
#-------------------------------------------------
add_library(rtmidi STATIC external/rtmidi/RtMidi.cpp)
target_include_directories(rtmidi PUBLIC external/rtmidi)
if(UNIX AND NOT APPLE)
    target_link_libraries(rtmidi PUBLIC asound pthread)
endif()
if(WIN32)
    target_link_libraries(rtmidi PUBLIC winmm)
endif()

#-------------------------------------------------
#  Main demo executable
#-------------------------------------------------
add_executable(sine_demo
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${IMGUI_BACKEND_SRC}               # include for IDE browsing
)

target_include_directories(sine_demo PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${LUAJIT_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
)

target_compile_definitions(sine_demo PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLEW)

target_link_libraries(sine_demo PRIVATE
    imgui_backend
    imgui
    glfw
    OpenGL::GL
    GLEW::GLEW
    ${LUAJIT_LIBRARIES}
    dl pthread
    ${PORTAUDIO_LIB}
    rtmidi
)

if(WIN32)
    target_link_libraries(sine_demo PRIVATE ole32 setupapi)
endif()

#-------------------------------------------------
#  Post‑build: copy Lua script(s)
#-------------------------------------------------
add_custom_command(TARGET sine_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sine_demo>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/sine_ui.lua
            $<TARGET_FILE_DIR:sine_demo>
)

#-------------------------------------------------
#  Convenience target:  `make run`
#-------------------------------------------------
add_custom_target(run
    COMMAND sine_demo
    DEPENDS sine_demo
    WORKING_DIRECTORY $<TARGET_FILE_DIR:sine_demo>)
