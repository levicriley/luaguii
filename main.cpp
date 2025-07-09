// main.cpp  –  Hybrid ImGui + Tcl sine-wave demo (GLEW loader)
//
// Build (Linux, from project root):
//   sudo apt install libglew-dev libglfw3-dev tcl-dev cmake swig build-essential
//   mkdir build && cd build
//   cmake .. && cmake --build .
//   ./sine_demo

#define IMGUI_IMPL_OPENGL_LOADER_GLEW   // use GLEW
#include <GL/glew.h>                   // must come before GLFW
#include <GLFW/glfw3.h>
#include <tcl.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

/* Helper: slider_float label varName min max ?step? */
static int SliderFloatVarCmd(ClientData, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[]) {
    if (objc < 5 || objc > 6) {
        Tcl_WrongNumArgs(ip, 1, objv, "label varName min max ?step?");
        return TCL_ERROR;
    }
    const char *label   = Tcl_GetString(objv[1]);
    const char *varName = Tcl_GetString(objv[2]);
    double vmin, vmax, step = 0.001;
    if (Tcl_GetDoubleFromObj(ip, objv[3], &vmin) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDoubleFromObj(ip, objv[4], &vmax) != TCL_OK) return TCL_ERROR;
    if (objc == 6 && Tcl_GetDoubleFromObj(ip, objv[5], &step) != TCL_OK) return TCL_ERROR;

    Tcl_Obj *varObj = Tcl_GetVar2Ex(ip, varName, nullptr, TCL_GLOBAL_ONLY);
    double val = vmin;
    if (varObj && Tcl_GetDoubleFromObj(ip, varObj, &val) != TCL_OK) val = vmin;

    float fval = static_cast<float>(val);
    bool changed = ImGui::SliderFloat(label, &fval,
                                      (float)vmin, (float)vmax,
                                      "%.3f", 0.0f);
    if (changed) {
        Tcl_SetVar2Ex(ip, varName, nullptr, Tcl_NewDoubleObj(fval), TCL_GLOBAL_ONLY);
    }
    Tcl_SetObjResult(ip, Tcl_NewDoubleObj(fval));
    return TCL_OK;
}

/* Helper: plot_sine amp freq ?samples? */
static int PlotSineCmd(ClientData, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[]) {
    if (objc != 3 && objc != 4) {
        Tcl_WrongNumArgs(ip, 1, objv, "amplitude frequency ?samples?");
        return TCL_ERROR;
    }
    double amp, freq;
    int samples = 512;
    if (Tcl_GetDoubleFromObj(ip, objv[1], &amp) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDoubleFromObj(ip, objv[2], &freq) != TCL_OK) return TCL_ERROR;
    if (objc == 4 && Tcl_GetIntFromObj(ip, objv[3], &samples) != TCL_OK) return TCL_ERROR;

    samples = std::clamp(samples, 2, 2048);
    static std::vector<float> buf;
    buf.resize(samples);

    const float twoPi = 6.28318530718f;
    for (int i = 0; i < samples; ++i) {
        float t = (float)i / (samples - 1);
        buf[i] = (float)(amp * std::sin(twoPi * freq * t));
    }
    ImGui::PlotLines("##sinewave", buf.data(), samples, 0,
                     nullptr, (float)-amp, (float)amp, ImVec2(-1,150));
    return TCL_OK;
}

// simple one-arg wrapper for igBegin/igEnd
static int BeginCmd(ClientData, Tcl_Interp* ip, int objc, Tcl_Obj *const objv[]) {
    if (objc!=2) { Tcl_WrongNumArgs(ip,1,objv,"name"); return TCL_ERROR; }
    const char* name = Tcl_GetString(objv[1]);
    ImGui::Begin(name);
    return TCL_OK;
}
static int EndCmd(ClientData, Tcl_Interp* ip, int objc, Tcl_Obj *const objv[]) {
    ImGui::End();
    return TCL_OK;
}


static void RegisterHelpers(Tcl_Interp *ip) {
    Tcl_CreateObjCommand(ip, "slider_float", SliderFloatVarCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(ip, "plot_sine",    PlotSineCmd,      nullptr, nullptr);
}

static void glfw_error_callback(int err, const char* desc) {
    std::fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}

int main() {
    // --- GLFW / OpenGL setup ---
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(960,540,"Tcl+ImGui Sine Demo",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // --- GLEW init ---
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "Failed to initialize GLEW\n");
        return 1;
    }

    // --- ImGui setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window,true);
    ImGui_ImplOpenGL3_Init("#version 130");
    // choose a non-black background
    ImVec4 clear_color = ImVec4(0.45f,0.55f,0.60f,1.00f);

    // --- Tcl interpreter setup ---
    Tcl_FindExecutable(nullptr);
    Tcl_Interp* interp = Tcl_CreateInterp();
    Tcl_Init(interp);
    // explicitly load the SWIG‐generated imgui_tcl extension
    if (Tcl_Eval(interp, "load ./imgui_tcl.so") != TCL_OK) {
        std::fprintf(stderr, "Failed to load imgui_tcl.so: %s\n", Tcl_GetStringResult(interp));
        return 1;
    }
    RegisterHelpers(interp);
    Tcl_CreateObjCommand(interp, "igBegin", BeginCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "igEnd",   EndCmd,   nullptr, nullptr);

    if (Tcl_EvalFile(interp,"sine_ui.tcl") != TCL_OK) {
        std::fprintf(stderr,"Error sourcing sine_ui.tcl: %s\n",Tcl_GetStringResult(interp));
        return 1;
    }

    // --- Main loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (Tcl_Eval(interp, "pre_frame") != TCL_OK) {
            fprintf(stderr, "Tcl pre_frame error: %s\n", Tcl_GetStringResult(interp));
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (Tcl_Eval(interp, "draw_ui") != TCL_OK) {
            std::fprintf(stderr, "Tcl draw_ui error: %s\n", Tcl_GetStringResult(interp));
        }

        ImGui::Render();
        int w,h;
        glfwGetFramebufferSize(window,&w,&h);
        glViewport(0,0,w,h);
        glClearColor(clear_color.x,clear_color.y,clear_color.z,clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (Tcl_Eval(interp, "post_frame") != TCL_OK) {
            fprintf(stderr, "Tcl post_frame error: %s\n", Tcl_GetStringResult(interp));
        }
        glfwSwapBuffers(window);
    }
    // --- Cleanup ---
    Tcl_DeleteInterp(interp);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
