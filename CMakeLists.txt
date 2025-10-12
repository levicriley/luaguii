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
#  Dear ImGui (vendored)
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
#  llama.cpp (submodule, optional)
#-------------------------------------------------
option(ENABLE_LLAMACPP "Build with llama.cpp inference support" ON)

if(ENABLE_LLAMACPP)
    # Uncomment to enable backends:
    # set(LLAMA_BLAS ON CACHE BOOL "" FORCE)
    # set(LLAMA_BLAS_VENDOR "OpenBLAS" CACHE STRING "" FORCE)
    # set(LLAMA_CUBLAS ON CACHE BOOL "" FORCE)
    # set(LLAMA_METAL ON CACHE BOOL "" FORCE)

    add_subdirectory(external/llama.cpp EXCLUDE_FROM_ALL)

    add_library(llama_cpp_interface INTERFACE)
    target_include_directories(llama_cpp_interface INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include)
    target_link_libraries(llama_cpp_interface INTERFACE llama)
endif()

#-------------------------------------------------
#  Main demo executable
#-------------------------------------------------
add_executable(sine_demo
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
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
    ${PORTAUDIO_LIB}
    rtmidi
)

if(UNIX AND NOT APPLE)
    target_link_libraries(sine_demo PRIVATE dl pthread)
endif()

if(WIN32)
    target_link_libraries(sine_demo PRIVATE ole32 setupapi)
endif()

if(ENABLE_LLAMACPP)
    target_link_libraries(sine_demo PRIVATE llama_cpp_interface)
endif()

#-------------------------------------------------
#  Post-build: copy Lua script(s)
#-------------------------------------------------
add_custom_command(TARGET sine_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sine_demo>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/sine_ui.lua
            $<TARGET_FILE_DIR:sine_demo>
)

#-------------------------------------------------
#  Post-build: copy models/ (GGUFs) next to the binary
#-------------------------------------------------
set(MODELS_DIR "${CMAKE_SOURCE_DIR}/models")
if(EXISTS "${MODELS_DIR}")
  add_custom_command(TARGET sine_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:sine_demo>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MODELS_DIR}"
            "$<TARGET_FILE_DIR:sine_demo>/models"
    COMMENT "Copying GGUF models to build output"
  )
endif()

#-------------------------------------------------
#  Convenience target:  `make run`
#-------------------------------------------------
add_custom_target(run
    COMMAND sine_demo
    DEPENDS sine_demo
    WORKING_DIRECTORY $<TARGET_FILE_DIR:sine_demo>)

#-------------------------------------------------
#  (Optional) Install rules — commented out
#-------------------------------------------------
# include(GNUInstallDirs)
# install(TARGETS sine_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# if(EXISTS "${MODELS_DIR}")
#   install(DIRECTORY "${MODELS_DIR}/" DESTINATION ${CMAKE_INSTALL_BINDIR}/models)
# endif()
# install(FILES ${CMAKE_SOURCE_DIR}/sine_ui.lua DESTINATION ${CMAKE_INSTALL_BINDIR})


# ---- Mini console-only llama demo ----
add_executable(mini_llama ${CMAKE_CURRENT_SOURCE_DIR}/mini_llama.cpp)
target_include_directories(mini_llama PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include)
target_link_libraries(mini_llama PRIVATE llama)

# Run it from the same out dir as sine_demo so models/ copy works
add_custom_target(run_mini
    COMMAND mini_llama "Say hi in one sentence."
    DEPENDS mini_llama
    WORKING_DIRECTORY $<TARGET_FILE_DIR:mini_llama>)

# Reuse the models/ copy step for mini_llama as well
set(MODELS_DIR "${CMAKE_SOURCE_DIR}/models")
if(EXISTS "${MODELS_DIR}")
  add_custom_command(TARGET mini_llama POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:mini_llama>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MODELS_DIR}"
            "$<TARGET_FILE_DIR:mini_llama>/models"
    COMMENT "Copying GGUF models to mini_llama output")
endif()
