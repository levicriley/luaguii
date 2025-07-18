/* main.cpp ─ ImGui + Tcl + PortAudio + RtMidi + core‑GL (shader) in ImGui
 * build:   needs GLEW, GLFW, Tcl, SWIG shim (imgui_tcl.so), PortAudio, RtMidi
 * author:  2025‑07‑18  */

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
#include <cstdint>   /* intptr_t */

/*──────────────────── 1. PortAudio test tone ───────────────────*/
struct Sine { double phase = 0.0, freq = 440.0; } gSine;

static int paCB(const void*, void* out,
                unsigned long frames,
                const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
                void* user)
{
    auto* s   = static_cast<Sine*>(user);
    float* buf= static_cast<float*>(out);
    double inc= 2.0 * M_PI * s->freq / 48'000.0;
    for (unsigned long i = 0; i < frames; ++i) {
        buf[i] = std::sin(s->phase);
        s->phase += inc;
        if (s->phase > 2 * M_PI) s->phase -= 2 * M_PI;
    }
    return paContinue;
}

static int Audio_SetFreq_Cmd(ClientData, Tcl_Interp* ip, int objc, Tcl_Obj* const ov[])
{
    if (objc != 2) { Tcl_WrongNumArgs(ip,1,ov,"freqHz"); return TCL_ERROR; }
    double f; if (Tcl_GetDoubleFromObj(ip,ov[1],&f) != TCL_OK) return TCL_ERROR;
    gSine.freq = f; return TCL_OK;
}

/*──────────────────── 2. ImGui helpers exposed to Tcl ─────────────────*/
static int SliderFloatVarCmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const ov[])
{
    if (objc < 5 || objc > 6) {
        Tcl_WrongNumArgs(ip,1,ov,"label var min max ?step?"); return TCL_ERROR;
    }
    const char* lbl = Tcl_GetString(ov[1]);
    const char* var = Tcl_GetString(ov[2]);
    double mn,mx,st = 0.001;
    if (Tcl_GetDoubleFromObj(ip,ov[3],&mn)!=TCL_OK) return TCL_ERROR;
    if (Tcl_GetDoubleFromObj(ip,ov[4],&mx)!=TCL_OK) return TCL_ERROR;
    if (objc==6 && Tcl_GetDoubleFromObj(ip,ov[5],&st)!=TCL_OK) return TCL_ERROR;

    Tcl_Obj* cur = Tcl_GetVar2Ex(ip,var,nullptr,TCL_GLOBAL_ONLY);
    double v = mn;
    if (cur && Tcl_GetDoubleFromObj(ip,cur,&v)!=TCL_OK) v = mn;
    float fv = static_cast<float>(v);

    if (ImGui::SliderFloat(lbl,&fv,(float)mn,(float)mx,"%.3f",0.f))
        Tcl_SetVar2Ex(ip,var,nullptr,Tcl_NewDoubleObj(fv),TCL_GLOBAL_ONLY);

    Tcl_SetObjResult(ip,Tcl_NewDoubleObj(fv));
    return TCL_OK;
}

static int PlotSineCmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const ov[])
{
    if (objc != 3 && objc != 4) {
        Tcl_WrongNumArgs(ip,1,ov,"amp freq ?samples?"); return TCL_ERROR;
    }
    double amp,freq; int n = 512;
    if (Tcl_GetDoubleFromObj(ip,ov[1],&amp)!=TCL_OK)  return TCL_ERROR;
    if (Tcl_GetDoubleFromObj(ip,ov[2],&freq)!=TCL_OK) return TCL_ERROR;
    if (objc==4 && Tcl_GetIntFromObj(ip,ov[3],&n)!=TCL_OK) return TCL_ERROR;

    n = std::clamp(n,2,2048);
    static std::vector<float> buf; buf.resize(n);
    const float tp = 6.2831853f;
    for (int i=0;i<n;++i){
        float t = (float)i/(n-1);
        buf[i]  = static_cast<float>(amp * std::sin(tp * freq * t));
    }
    ImGui::PlotLines("##sine",buf.data(),n,0,nullptr,
                     (float)-amp,(float)amp,ImVec2(-1,150));
    return TCL_OK;
}

/* one‑arg wrappers so Tcl can call igBegin / igEnd */
static int BeginCmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const ov[])
{ if(objc!=2){Tcl_WrongNumArgs(ip,1,ov,"title"); return TCL_ERROR;}
  ImGui::Begin(Tcl_GetString(ov[1])); return TCL_OK; }
static int EndCmd(ClientData, Tcl_Interp*, int, Tcl_Obj* const*) { ImGui::End(); return TCL_OK; }

/* convenience: default‑size button that just returns the bool */
static int DefaultButton_Cmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const ov[])
{
    if (objc != 2) { Tcl_WrongNumArgs(ip,1,ov,"label"); return TCL_ERROR; }
    bool pressed = ImGui::Button(Tcl_GetString(ov[1]));
    Tcl_SetObjResult(ip, Tcl_NewBooleanObj(pressed));
    return TCL_OK;
}

/*──────────────────── 3. FBO + shader triangle for core GL ─────────────*/
struct MiniFBO {
    GLuint fbo=0,tex=0,rbo=0; int w=0,h=0;
    void ensure(int side){
        if (side<=w && side<=h) return;
        w=h=side;
        if(!fbo) glGenFramebuffers (1,&fbo);
        if(!tex) glGenTextures    (1,&tex);
        if(!rbo) glGenRenderbuffers(1,&rbo);

        glBindTexture(GL_TEXTURE_2D,tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

        glBindRenderbuffer(GL_RENDERBUFFER,rbo);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);

        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,tex,0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            std::fprintf(stderr,"FBO incomplete!\n");

        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
} gMini;

/* tiny program that uses gl_VertexID – no VBO necessary */
static GLuint gProg = 0, gVAO = 0;
static void ensureTriangleProgram()
{
    if (gProg) return;

    auto compile = [](GLenum type, const char* src)->GLuint{
        GLuint s = glCreateShader(type);
        glShaderSource(s,1,&src,nullptr); glCompileShader(s);
        GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){ char log[512]; glGetShaderInfoLog(s,512,nullptr,log);
                 std::fprintf(stderr,"shader err: %s\n",log); }
        return s;
    };

    const char* vs =
        "#version 330 core\n"
        "const vec2 v[3] = vec2[3](vec2( 0.0, 0.8), vec2(-0.8,-0.8), vec2(0.8,-0.8));\n"
        "out vec3 col;\n"
        "void main(){\n"
        "  gl_Position = vec4(v[gl_VertexID],0,1);\n"
        "  col = (gl_VertexID==0)? vec3(1,0.5,0): (gl_VertexID==1)? vec3(0,0.6,1): vec3(0,1,0.4);\n"
        "}";
    const char* fs =
        "#version 330 core\n"
        "in  vec3 col; out vec4 FragColor;\n"
        "void main(){ FragColor = vec4(col,1); }";

    gProg = glCreateProgram();
    glAttachShader(gProg, compile(GL_VERTEX_SHADER  ,vs));
    glAttachShader(gProg, compile(GL_FRAGMENT_SHADER,fs));
    glLinkProgram(gProg);
    GLint ok; glGetProgramiv(gProg,GL_LINK_STATUS,&ok);
    if(!ok){ char log[512]; glGetProgramInfoLog(gProg,512,nullptr,log);
             std::fprintf(stderr,"link err: %s\n",log); }

    glGenVertexArrays(1,&gVAO);
}

static void renderTriangle(int side)
{
    ensureTriangleProgram();
    gMini.ensure(side);

    glBindFramebuffer(GL_FRAMEBUFFER,gMini.fbo);
    glViewport(0,0,side,side);
    glClearColor(0.1f,0.12f,0.15f,1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gProg);
    glBindVertexArray(gVAO);
    glDrawArrays(GL_TRIANGLES,0,3);

    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static int GL_Triangle_Cmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const ov[])
{
    int side = 256;
    if (objc == 2) {
        if (Tcl_GetIntFromObj(ip,ov[1],&side)!=TCL_OK) return TCL_ERROR;
    } else if (objc != 1) {
        Tcl_WrongNumArgs(ip,1,ov,"?pixels?"); return TCL_ERROR;
    }

    renderTriangle(side);
    ImGui::Image((ImTextureID)(intptr_t)gMini.tex,
                 ImVec2((float)side,(float)side),
                 ImVec2(0,1), ImVec2(1,0)); /* flip vertically */

    return TCL_OK;
}
/*────────────────────────  cube : shader + VAO  ───────────────────────*/
static GLuint gCubeProg = 0, gCubeVAO = 0, gCubeVBO = 0;
static MiniFBO gCubeFBO;                                // reuse helper

// --- replace the two older helpers -----------------------------------------------------

static const char* cubeVS = R"(
    #version 330 core
    layout(location=0) in vec3 p;

    uniform float angle;   // radians
    uniform float aspect;  // side/side == 1, but keep in case you change FBO

    // very small helper: column‑major mat4 from 16 floats -----------------
    mat4 m4(float a0,float a1,float a2,float a3,
            float b0,float b1,float b2,float b3,
            float c0,float c1,float c2,float c3,
            float d0,float d1,float d2,float d3)
    { return mat4(a0,a1,a2,a3, b0,b1,b2,b3, c0,c1,c2,c3, d0,d1,d2,d3); }

    void main()
    {
        /* 1) model: rotate & scale */
        mat4 R = m4(  cos(angle), 0, -sin(angle), 0,
                       0,         1,  0,          0,
                       sin(angle), 0,  cos(angle), 0,
                       0,0,0,1);
        mat4 S = m4( 0.6,0,0,0,  0,0.6,0,0,  0,0,0.6,0,  0,0,0,1);

        /* 2) view: simple camera at +Z looking toward origin */
        mat4 V = m4( 1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,-3,1);

        /* 3) projection:  60° vertical FOV, 0.1–10 range */
        float f = 1.0 / tan(radians(60.0)/2.0);
        mat4 P = m4( f/aspect,0,0,0,
                     0,f,0,0,
                     0,0,-(10+0.1)/(10-0.1),-1,
                     0,0,-(2*10*0.1)/(10-0.1),0);

        gl_Position = P * V * R * S * vec4(p,1);
    }
)";

static const char* cubeFS = R"(
    #version 330 core
    out vec4 color;
    void main(){ color = vec4(0.7,0.8,1.0,1); }   // light blue
)";

static void ensureCubeProgram()
{
    if (gCubeProg) return;

    auto sh = [&](GLenum t,const char* s){
        GLuint id = glCreateShader(t);
        glShaderSource(id,1,&s,nullptr); glCompileShader(id);
        GLint ok; glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
        if(!ok){ char log[256]; glGetShaderInfoLog(id,256,nullptr,log);
                 fprintf(stderr,"%s\n",log); }
        return id;
    };

    gCubeProg = glCreateProgram();
    glAttachShader(gCubeProg, sh(GL_VERTEX_SHADER  , cubeVS));
    glAttachShader(gCubeProg, sh(GL_FRAGMENT_SHADER, cubeFS));
    glLinkProgram(gCubeProg);

    /* ---------- NEW: upload 36‑vertex cube ---------- */
    const float verts[] = {
        // front
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  -1,-1, 1,  1, 1, 1,  -1, 1, 1,
        // back
        -1,-1,-1, -1, 1,-1,  1, 1,-1,  -1,-1,-1,  1, 1,-1,   1,-1,-1,
        // left
        -1,-1,-1, -1,-1, 1, -1, 1, 1,  -1,-1,-1, -1, 1, 1,  -1, 1,-1,
        // right
         1,-1,-1,  1, 1,-1,  1, 1, 1,   1,-1,-1,  1, 1, 1,   1,-1, 1,
        // top
        -1, 1,-1, -1, 1, 1,  1, 1, 1,  -1, 1,-1,  1, 1, 1,   1, 1,-1,
        // bottom
        -1,-1,-1,  1,-1,-1,  1,-1, 1,  -1,-1,-1,  1,-1, 1,  -1,-1, 1
    };

    glGenVertexArrays(1,&gCubeVAO);
    glGenBuffers     (1,&gCubeVBO);

    glBindVertexArray(gCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gCubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    /* ------------------------------------------------- */
}


/* draw the cube into an FBO of size ‹side›×‹side›, rotated ‹angle› radians */
static void renderCube(int side,float angle)
{
    ensureCubeProgram();
    gCubeFBO.ensure(side);

    glBindFramebuffer(GL_FRAMEBUFFER,gCubeFBO.fbo);
    glViewport(0,0,side,side);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f,0.12f,0.16f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glUseProgram(gCubeProg);
    glUniform1f(glGetUniformLocation(gCubeProg,"angle"),angle);
    glUniform1f(glGetUniformLocation(gCubeProg,"aspect"),1.0f); // square FBO

    glBindVertexArray(gCubeVAO);
    glDrawArrays(GL_TRIANGLES,0,36);

    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDisable(GL_DEPTH_TEST);
}


/* Tcl command:  gl_cube <pixels> <angle‑rad>   (second arg required) */
static int GL_Cube_Cmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const ov[])
{
    if(objc!=3){ Tcl_WrongNumArgs(ip,1,ov,"sidePx angleRad"); return TCL_ERROR; }
    int side; double ang;
    if(Tcl_GetIntFromObj(ip,ov[1],&side)!=TCL_OK) return TCL_ERROR;
    if(Tcl_GetDoubleFromObj(ip,ov[2],&ang)!=TCL_OK) return TCL_ERROR;

    renderCube(side,(float)ang);
    ImGui::Image((ImTextureID)(intptr_t)gCubeFBO.tex,
                 ImVec2((float)side,(float)side),
                 ImVec2(0,1), ImVec2(1,0));   // flip vertically
    return TCL_OK;
}

/*──────────────────── 4. RtMidi helper ─────────────────────────*/
static int Midi_List_Cmd(ClientData,Tcl_Interp* ip,int,Tcl_Obj* const*)
{
    RtMidiIn in;
    Tcl_Obj* list = Tcl_NewListObj(0,nullptr);
    for (unsigned p=0;p<in.getPortCount();++p)
        Tcl_ListObjAppendElement(ip,list,
            Tcl_NewStringObj(in.getPortName(p).c_str(),-1));
    Tcl_SetObjResult(ip,list);
    return TCL_OK;
}

/*──────────────────── 5. Register all Tcl commands ─────────────*/
static void registerAll(Tcl_Interp* ip)
{
    Tcl_CreateObjCommand(ip,"slider_float",SliderFloatVarCmd,nullptr,nullptr);
    Tcl_CreateObjCommand(ip,"plot_sine",   PlotSineCmd,      nullptr,nullptr);
    Tcl_CreateObjCommand(ip,"gl_triangle", GL_Triangle_Cmd,  nullptr,nullptr);

    Tcl_CreateObjCommand(ip,"igBegin",BeginCmd,nullptr,nullptr);
    Tcl_CreateObjCommand(ip,"igEnd",  EndCmd,  nullptr,nullptr);

    Tcl_CreateObjCommand(ip,"audio_set_freq",Audio_SetFreq_Cmd,nullptr,nullptr);
    Tcl_CreateObjCommand(ip,"midi_ports",    Midi_List_Cmd,    nullptr,nullptr);

    Tcl_CreateObjCommand(ip,"imgui_button",  DefaultButton_Cmd,nullptr,nullptr);
    /* in registerAll(...) add one line */
    Tcl_CreateObjCommand(ip,"gl_cube",GL_Cube_Cmd,nullptr,nullptr);

}

/*──────────────────── 6. Entry point ───────────────────────────*/
static void glfwErr(int e,const char* d){ std::fprintf(stderr,"GLFW %d: %s\n",e,d); }

int main()
{
    /* PortAudio init */
    Pa_Initialize();
    PaStream* stream;
    Pa_OpenDefaultStream(&stream,0,1,paFloat32,48000,256,paCB,&gSine);
    Pa_StartStream(stream);

    /* GLFW / OpenGL (core profile) */
    glfwSetErrorCallback(glfwErr);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(960,540,"Core‑GL in ImGui",nullptr,nullptr);
    if (!win){ glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win); glfwSwapInterval(1);
    glewExperimental = GL_TRUE; glewInit();

    /* ImGui */
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win,true);
    ImGui_ImplOpenGL3_Init("#version 330");        /* GLSL 3.30 core */
    ImVec4 clear={0.16f,0.18f,0.22f,1};

    /* Tcl */
    Tcl_FindExecutable(nullptr);
    Tcl_Interp* ip = Tcl_CreateInterp(); Tcl_Init(ip);
    Tcl_Eval(ip,"load ./imgui_tcl.so");
    registerAll(ip);
    if (Tcl_EvalFile(ip,"sine_ui.tcl") != TCL_OK) {
        std::fprintf(stderr,"Tcl error: %s\n",Tcl_GetStringResult(ip)); return 1;
    }

    /* main loop */
    while(!glfwWindowShouldClose(win)){
        glfwPollEvents();
        Tcl_Eval(ip,"pre_frame");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        Tcl_Eval(ip,"draw_ui");

        ImGui::Render();
        int w,h; glfwGetFramebufferSize(win,&w,&h);
        glViewport(0,0,w,h);
        glClearColor(clear.x,clear.y,clear.z,clear.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        Tcl_Eval(ip,"post_frame");
        glfwSwapBuffers(win);
    }

    /* cleanup */
    Pa_StopStream(stream); Pa_CloseStream(stream); Pa_Terminate();
    Tcl_DeleteInterp(ip);
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win); glfwTerminate();
    return 0;
}
