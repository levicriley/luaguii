#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include <lua.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <portaudio.h>            // ← pull in the PortAudio types

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <sol/sol.hpp>    // ← now works!
#include <RtMidi.h>
// …rest of your code…


//─────────────── PortAudio sine generator ────────────────
struct Sine { double phase=0, freq=440; } gSine;
static int paCallback(const void*, void* out, unsigned long n,
                      const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*)
{
    auto* buf = static_cast<float*>(out);
    double inc = 2*M_PI*gSine.freq/48000.0;
    for(unsigned i=0;i<n;++i){
        buf[i] = std::sin(gSine.phase);
        gSine.phase += inc;
        if(gSine.phase>2*M_PI) gSine.phase -= 2*M_PI;
    }
    return paContinue;
}

//─────────────── ImGui triangle + cube via FBO + shaders ────────────────
static GLuint triProg=0, triVAO=0, cubeProg=0, cubeVAO=0;
struct FBO { GLuint fbo=0, tex=0, rbo=0; int w=0,h=0;
    void ensure(int s){
      if(s<=w && s<=h) return; w=h=s;
      if(!fbo) glGenFramebuffers(1,&fbo);
      if(!tex) glGenTextures(1,&tex);
      if(!rbo) glGenRenderbuffers(1,&rbo);
      glBindTexture(GL_TEXTURE_2D,tex);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glBindRenderbuffer(GL_RENDERBUFFER,rbo);
      glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,s,s);
      glBindFramebuffer(GL_FRAMEBUFFER,fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,rbo);
      glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
} gFBO;

static GLuint compileShader(GLenum type,const char*src){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    return s;
}

static void makeTriangle(){
    if(triProg) return;
    const char* vs = R"(
      #version 330 core
      const vec2 pts[3] = vec2[3](vec2(0,0.8),vec2(-0.8,-0.8),vec2(0.8,-0.8));
      void main(){ gl_Position=vec4(pts[gl_VertexID],0,1); }
    )";
    const char* fs = R"(
      #version 330 core
      out vec4 o; void main(){ o=vec4(1,0.5,0.2,1); }
    )";
    triProg=glCreateProgram();
    glAttachShader(triProg,compileShader(GL_VERTEX_SHADER,vs));
    glAttachShader(triProg,compileShader(GL_FRAGMENT_SHADER,fs));
    glLinkProgram(triProg);
    glGenVertexArrays(1,&triVAO);
}

static void renderTriangle(int side){
    makeTriangle();
    gFBO.ensure(side);
    glBindFramebuffer(GL_FRAMEBUFFER,gFBO.fbo);
    glViewport(0,0,side,side);
    glClearColor(0.1f,0.12f,0.15f,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(triProg);
    glBindVertexArray(triVAO);
    glDrawArrays(GL_TRIANGLES,0,3);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    ImGui::Image((ImTextureID)(intptr_t)gFBO.tex,ImVec2(side,side),ImVec2(0,1),ImVec2(1,0));
}

static void makeCube(){
    if(cubeProg) return;
    const char* vs = R"(
      #version 330 core
      layout(location=0) in vec3 P;
      uniform float angle;
      uniform float aspect;
      mat4 m4(float a0,float a1,float a2,float a3,
              float b0,float b1,float b2,float b3,
              float c0,float c1,float c2,float c3,
              float d0,float d1,float d2,float d3){
        return mat4(a0,a1,a2,a3,b0,b1,b2,b3,c0,c1,c2,c3,d0,d1,d2,d3);
      }
      void main(){
        mat4 R = m4(cos(angle),0,-sin(angle),0, 0,1,0,0, sin(angle),0,cos(angle),0, 0,0,0,1);
        mat4 S = m4(0.6,0,0,0, 0,0.6,0,0, 0,0,0.6,0, 0,0,0,1);
        mat4 V = m4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-3,1);
        float f=1.0/tan(radians(60)/2);
        mat4 P = m4(f/aspect,0,0,0, 0,f,0,0, 0,0,-(10+0.1)/(10-0.1),-1, 0,0,-(2*10*0.1)/(10-0.1),0);
        gl_Position = P*V*R*S*vec4(P,1);
      }
    )";
    const char* fs = R"(
      #version 330 core
      out vec4 o; void main(){ o=vec4(0.7,0.8,1,1); }
    )";
    cubeProg=glCreateProgram();
    glAttachShader(cubeProg,compileShader(GL_VERTEX_SHADER,vs));
    glAttachShader(cubeProg,compileShader(GL_FRAGMENT_SHADER,fs));
    glLinkProgram(cubeProg);
    glGenVertexArrays(1,&cubeVAO);
}

static void renderCube(int side,float angle){
    makeCube();
    gFBO.ensure(side);
    glBindFramebuffer(GL_FRAMEBUFFER,gFBO.fbo);
    glEnable(GL_DEPTH_TEST);
    glViewport(0,0,side,side);
    glClearColor(0.1f,0.12f,0.16f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(cubeProg);
    glUniform1f(glGetUniformLocation(cubeProg,"angle"),angle);
    glUniform1f(glGetUniformLocation(cubeProg,"aspect"),1.0f);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES,0,36);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    ImGui::Image((ImTextureID)(intptr_t)gFBO.tex,ImVec2(side,side),ImVec2(0,1),ImVec2(1,0));
}

//─────────────── Lua helpers ────────────────
static int l_audio_set_freq(lua_State* L){
    gSine.freq = luaL_checknumber(L,1);
    return 0;
}
static int l_list_midi(lua_State* L){
    RtMidiIn in; lua_newtable(L);
    for(unsigned i=0;i<in.getPortCount();++i){
        lua_pushinteger(L,i+1);
        lua_pushstring(L,in.getPortName(i).c_str());
        lua_settable(L,-3);
    }
    return 1;
}

//─────────────── Entry point ────────────────
int main(){
    // PortAudio
    Pa_Initialize();
    PaStream* st;
    Pa_OpenDefaultStream(&st,0,1,paFloat32,48000,256,paCallback,nullptr);
    Pa_StartStream(st);

    // GLFW + GL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(960,540,"ImGui LuaJIT Demo",nullptr,nullptr);
    glfwMakeContextCurrent(win);
    glewExperimental=1; glewInit();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(win,true);
    ImGui_ImplOpenGL3_Init("#version 330");

    //--- Raw LuaJIT path (Option 2) -----------------------------
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_pushcfunction(L,l_audio_set_freq);  lua_setglobal(L,"audio_set_freq");
    lua_pushcfunction(L,l_list_midi);       lua_setglobal(L,"list_midi_ports");
    luaL_dofile(L,"gui.lua");                    // your UI

    //--- or Option 3: Sol‑3 -----------------------------
    sol::state lua;
    lua.open_libraries(sol::lib::base,sol::lib::math);
    lua.set_function("audio_set_freq",[](double f){ gSine.freq=f; });
    lua.set_function("list_midi_ports",[](){
      RtMidiIn in; std::vector<std::string> v;
      for(unsigned i=0;i<in.getPortCount();++i)
        v.push_back(in.getPortName(i));
      return v;
    });
    lua.script_file("gui.lua");
    sol::function draw_ui = lua["draw_ui"];

    // main loop
    while(!glfwWindowShouldClose(win)){
      glfwPollEvents();
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      // choose one:
      // lua_pcall(L, ... );
      draw_ui();                // Sol‑3

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      glfwSwapBuffers(win);
    }

    Pa_StopStream(st);
    Pa_CloseStream(st);
    Pa_Terminate();
    return 0;
}
