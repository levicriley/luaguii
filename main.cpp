/* main.cpp – ImGui + Tcl + PortAudio + RtMidi                                      */
/* ------------------------------------------------------------------------------- */
#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <portaudio.h>
#include "RtMidi.h"

#include <tcl.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

/*─────────────────────  1. PortAudio sine generator  ───────────────────*/
struct SineState { double phase = 0.0; double freq = 440.0; };
static SineState gSine;

static int paSineCB(const void*, void* out,
                    unsigned long frames,
                    const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
                    void* user)
{
    auto* st  = static_cast<SineState*>(user);
    float* buf= static_cast<float*>(out);
    double inc= 2.0 * M_PI * st->freq / 48000.0;
    for(unsigned long i=0;i<frames;++i){
        buf[i] = std::sin(st->phase);
        st->phase += inc;
        if(st->phase > 2*M_PI) st->phase -= 2*M_PI;
    }
    return paContinue;
}

/*─────────────────────  2. ImGui helpers usable from Tcl  ──────────────*/
static int SliderFloatVarCmd(ClientData, Tcl_Interp* ip, int objc, Tcl_Obj* const objv[])
{
    if(objc<5 || objc>6){
        Tcl_WrongNumArgs(ip,1,objv,"label varName min max ?step?"); return TCL_ERROR;
    }
    const char* label   = Tcl_GetString(objv[1]);
    const char* varName = Tcl_GetString(objv[2]);
    double vmin,vmax,step=0.001;
    if(Tcl_GetDoubleFromObj(ip,objv[3],&vmin)!=TCL_OK) return TCL_ERROR;
    if(Tcl_GetDoubleFromObj(ip,objv[4],&vmax)!=TCL_OK) return TCL_ERROR;
    if(objc==6 && Tcl_GetDoubleFromObj(ip,objv[5],&step)!=TCL_OK) return TCL_ERROR;

    Tcl_Obj* varObj = Tcl_GetVar2Ex(ip,varName,nullptr,TCL_GLOBAL_ONLY);
    double   val    = vmin;
    if(varObj && Tcl_GetDoubleFromObj(ip,varObj,&val)!=TCL_OK) val=vmin;

    float fval = static_cast<float>(val);
    bool changed = ImGui::SliderFloat(label,&fval,
                    static_cast<float>(vmin),static_cast<float>(vmax),"%.3f",0.0f);
    if(changed){
        Tcl_SetVar2Ex(ip,varName,nullptr,Tcl_NewDoubleObj(fval),TCL_GLOBAL_ONLY);
    }
    Tcl_SetObjResult(ip,Tcl_NewDoubleObj(fval));
    return TCL_OK;
}

static int PlotSineCmd(ClientData, Tcl_Interp* ip, int objc, Tcl_Obj* const objv[])
{
    if(objc!=3 && objc!=4){
        Tcl_WrongNumArgs(ip,1,objv,"amplitude frequency ?samples?"); return TCL_ERROR;
    }
    double amp,freq; int samples=512;
    if(Tcl_GetDoubleFromObj(ip,objv[1],&amp)!=TCL_OK)  return TCL_ERROR;
    if(Tcl_GetDoubleFromObj(ip,objv[2],&freq)!=TCL_OK)return TCL_ERROR;
    if(objc==4 && Tcl_GetIntFromObj(ip,objv[3],&samples)!=TCL_OK)   return TCL_ERROR;

    samples = std::clamp(samples,2,2048);
    static std::vector<float> buf; buf.resize(samples);
    const float twoPi = 6.28318530718f;
    for(int i=0;i<samples;++i){
        float t = (float)i/(samples-1);
        buf[i]  = static_cast<float>(amp*std::sin(twoPi*freq*t));
    }
    ImGui::PlotLines("##sine",buf.data(),samples,0,nullptr,
                     (float)-amp,(float)amp,ImVec2(-1,150));
    return TCL_OK;
}

/* one-arg wrappers for ImGui::Begin / End so Tcl side uses igBegin/igEnd */
static int BeginCmd(ClientData, Tcl_Interp* ip, int objc, Tcl_Obj* const objv[])
{
    if(objc!=2){ Tcl_WrongNumArgs(ip,1,objv,"name"); return TCL_ERROR; }
    ImGui::Begin(Tcl_GetString(objv[1])); return TCL_OK;
}
static int EndCmd(ClientData, Tcl_Interp*, int, Tcl_Obj* const*){ ImGui::End(); return TCL_OK; }

/*─────────────────────  3. Tcl ↔ C glue for audio & MIDI  ──────────────*/
static int Audio_SetFreq_Cmd(ClientData, Tcl_Interp* ip, int objc, Tcl_Obj* const objv[])
{
    if(objc!=2){ Tcl_WrongNumArgs(ip,1,objv,"freqHz"); return TCL_ERROR; }
    double f; if(Tcl_GetDoubleFromObj(ip,objv[1],&f)!=TCL_OK) return TCL_ERROR;
    gSine.freq = f; return TCL_OK;
}

static int Midi_List_Cmd(ClientData, Tcl_Interp* ip, int, Tcl_Obj* const*)
{
    RtMidiIn in;
    Tcl_Obj* list = Tcl_NewListObj(0,nullptr);
    for(unsigned int p=0;p<in.getPortCount();++p)
        Tcl_ListObjAppendElement(ip,list,
            Tcl_NewStringObj(in.getPortName(p).c_str(),-1));
    Tcl_SetObjResult(ip,list);
    return TCL_OK;
}

/*─────────────────────  4. Helper to register everything  ──────────────*/
static void RegisterHelpers(Tcl_Interp* ip)
{
    Tcl_CreateObjCommand(ip,"slider_float",SliderFloatVarCmd,nullptr,nullptr);
    Tcl_CreateObjCommand(ip,"plot_sine",   PlotSineCmd,      nullptr,nullptr);

    Tcl_CreateObjCommand(ip,"igBegin",BeginCmd,nullptr,nullptr);
    Tcl_CreateObjCommand(ip,"igEnd",  EndCmd,  nullptr,nullptr);

    Tcl_CreateObjCommand(ip,"audio_set_freq",Audio_SetFreq_Cmd,nullptr,nullptr);
    Tcl_CreateObjCommand(ip,"midi_ports",    Midi_List_Cmd,    nullptr,nullptr);
}

/*─────────────────────  5. Main application  ───────────────────*/
static void glfw_error_callback(int e,const char* d){ fprintf(stderr,"GLFW %d: %s\n",e,d); }

int main()
{
    /* PortAudio init */
    Pa_Initialize();
    PaStream* stream;
    Pa_OpenDefaultStream(&stream,0,1,paFloat32,48000,256,paSineCB,&gSine);
    Pa_StartStream(stream);

    /* GLFW + OpenGL */
    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
    GLFWwindow* win = glfwCreateWindow(960,540,"Audio/MIDI demo",nullptr,nullptr);
    if(!win){ glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win); glfwSwapInterval(1);
    glewExperimental = GL_TRUE; glewInit();

    /* ImGui */
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win,true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImVec4 bg = {0.15f,0.18f,0.20f,1.0f};

    /* Tcl */
    Tcl_FindExecutable(nullptr);
    Tcl_Interp* ip = Tcl_CreateInterp(); Tcl_Init(ip);
    Tcl_Eval(ip,"load ./imgui_tcl.so");         /* SWIG module */
    RegisterHelpers(ip);
    if(Tcl_EvalFile(ip,"sine_ui.tcl")!=TCL_OK){
        fprintf(stderr,"Tcl error: %s\n",Tcl_GetStringResult(ip)); return 1;
    }

    /* Main loop */
    while(!glfwWindowShouldClose(win)){
        glfwPollEvents();
        Tcl_Eval(ip,"pre_frame");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        Tcl_Eval(ip,"draw_ui");

        ImGui::Render();
        int w,h; glfwGetFramebufferSize(win,&w,&h);
        glViewport(0,0,w,h); glClearColor(bg.x,bg.y,bg.z,bg.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        Tcl_Eval(ip,"post_frame");
        glfwSwapBuffers(win);
    }

    /* Cleanup */
    Pa_StopStream(stream); Pa_CloseStream(stream); Pa_Terminate();
    Tcl_DeleteInterp(ip);
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win); glfwTerminate();
    return 0;
}
