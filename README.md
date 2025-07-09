\
# Tcl × Dear ImGui – Sine‑Wave Demo (Full CMake + cimgui submodule)

This repo shows how to:

* Add **cimgui** as a Git sub‑module (`external/cimgui`).
* Auto‑generate a **SWIG Tcl extension** that surfaces the full Dear ImGui API (`imgui_tcl`).
* Keep the high‑frequency render loop in C++ while scripting UI/state in Tcl.
* Plot a live sine‑wave whose amplitude and frequency are bound to Tcl variables.

## Quick Start

```bash
git clone --recursive https://github.com/YOU/tcl-imgui-sine-demo.git
cd tcl-imgui-sine-demo
mkdir build && cd build
cmake ..          # configure & build everything (cimgui, SWIG module, demo app)
cmake --build .   # or: make -j$(nproc)
./runtime/sine_demo   # sliders + sine‑wave UI
```

### Dependencies

| Ubuntu | macOS (brew) | Windows (MSYS‑2 / MinGW) |
|---|---|---|
| `sudo apt install build-essential cmake swig git tcl-dev libglfw3-dev libgl1-mesa-dev` | `brew install cmake swig tcl-tk glfw` | `pacman -S mingw-w64-x86_64-{toolchain,cmake,swig,tcl,glfw}` |

## Editing the UI

`runtime/sine_ui.tcl` is copied next to the executable at build time.  
Open it in your editor, hit **save**, and the running app auto‑reloads it on the next frame (see `post_frame`).

Have fun! 🎛️📈
