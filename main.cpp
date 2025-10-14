#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <portaudio.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui-knobs.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>

/*──────────────────── Audio ───────────────────*/
struct Sine { double phase=0.0, freq=440.0; } gSine;
static int paCB(const void*, void* out, unsigned long frames,
                const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* user){
    auto*s=(Sine*)user; float*buf=(float*)out; const double inc=2.0*M_PI*s->freq/48000.0;
    for(unsigned long i=0;i<frames;++i){ buf[i]=std::sin(s->phase); s->phase+=inc; if(s->phase>2*M_PI)s->phase-=2*M_PI; }
    return paContinue;
}

/*──────────────────── Mini FBO ───────────────────*/
struct MiniFBO {
    GLuint fbo=0,tex=0,rbo=0; int w=0,h=0;
    void ensure(int side){
        side = std::max(1, side);
        if (fbo && side<=w && side<=h) return;
        w=h=side;
        if(!fbo) glGenFramebuffers(1,&fbo);
        if(!tex) glGenTextures(1,&tex);
        if(!rbo) glGenRenderbuffers(1,&rbo);

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

/*──────────────────── Torus Shader (ray-march) ───────────────────*/
static MiniFBO gLuaTorusFBO;
static GLuint  gTorusProg=0, gTorusVAO=0;

static const char* kTorusVS = R"(
#version 330
const vec2 V[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
out vec2 uv; void main(){ vec2 p=V[gl_VertexID]; gl_Position=vec4(p,0,1); uv=p*0.5+0.5; }
)";

static const char* kTorusFS = R"(
#version 330
in vec2 uv; out vec4 color;
uniform float yaw,pitch,R,r;
float sdTorus(vec3 p,vec2 t){ vec2 q=vec2(length(p.xz)-t.x,p.y); return length(q)-t.y; }
mat3 rotY(float a){ float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }
mat3 rotX(float a){ float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }
vec3 nrm(vec3 p,vec2 T){ float e=.001;
  vec3 n=vec3(sdTorus(p+vec3(e,0,0),T)-sdTorus(p-vec3(e,0,0),T),
              sdTorus(p+vec3(0,e,0),T)-sdTorus(p-vec3(0,e,0),T),
              sdTorus(p+vec3(0,0,e),T)-sdTorus(p-vec3(0,0,e),T)); return normalize(n); }
void main(){
  vec2 p=uv*2-1; vec3 ro=vec3(0,0,3), rd=normalize(vec3(p,-1.5));
  mat3 RY=rotY(yaw), RX=rotX(pitch);
  float t=0; bool hit=false; vec2 T=vec2(R,r);
  for(int i=0;i<96;i++){ vec3 pos=RX*(RY*(ro+rd*t)); float d=sdTorus(pos,T);
    if(d<.001){hit=true;break;} t+=d*.9; if(t>20)break; }
  vec3 col=vec3(.16,.18,.22);
  if(hit){ vec3 pos=RX*(RY*(ro+rd*t)); vec3 n=nrm(pos,T);
    vec3 l=normalize(vec3(.5,.8,.3)); float diff=max(dot(n,l),0);
    float spec=pow(max(dot(reflect(-l,n),-rd),0),32); col=vec3(.70,.82,1.00)*(.25+.75*diff)+.25*spec; }
  color=vec4(col,1);
}
)";

static void ensureTorusProgram(){
    if (gTorusProg) return;
    auto sh=[](GLenum t,const char*s){ GLuint id=glCreateShader(t);
      glShaderSource(id,1,&s,nullptr); glCompileShader(id);
      GLint ok; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
      if(!ok){ char log[512]; glGetShaderInfoLog(id,512,nullptr,log); std::fprintf(stderr,"[torus] %s\n",log); }
      return id; };
    GLuint vs=sh(GL_VERTEX_SHADER,kTorusVS), fs=sh(GL_FRAGMENT_SHADER,kTorusFS);
    gTorusProg=glCreateProgram(); glAttachShader(gTorusProg,vs); glAttachShader(gTorusProg,fs);
    glLinkProgram(gTorusProg); glDeleteShader(vs); glDeleteShader(fs);
    glGenVertexArrays(1,&gTorusVAO);
}

static void renderTorusInto(MiniFBO& fbo,int side,float yaw,float pitch,float R,float r){
    ensureTorusProgram(); fbo.ensure(side);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo.fbo);
    glViewport(0,0,side,side);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gTorusProg);
    glUniform1f(glGetUniformLocation(gTorusProg,"yaw"),yaw);
    glUniform1f(glGetUniformLocation(gTorusProg,"pitch"),pitch);
    glUniform1f(glGetUniformLocation(gTorusProg,"R"),R);
    glUniform1f(glGetUniformLocation(gTorusProg,"r"),r);
    glBindVertexArray(gTorusVAO);
    glDrawArrays(GL_TRIANGLES,0,3);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

/*──────────────────── Lua bindings ───────────────────*/
// knob(label, val, min, max [, step=0.01 [, size=56]]) -> new_val
static int lua_knob_float(lua_State* L){
    const char* label = luaL_checkstring(L,1);
    float v     = (float)luaL_checknumber(L,2);
    float vmin  = (float)luaL_optnumber (L,3,0.0);
    float vmax  = (float)luaL_optnumber (L,4,1.0);
    float step  = (float)luaL_optnumber (L,5,0.01);
    float size  = (float)luaL_optnumber (L,6,56.0f);
    // correct order: speed, format, VARIANT, size
    ImGuiKnobs::Knob(label, &v, vmin, vmax, step, "%.3f", ImGuiKnobVariant_Tick, size);
    lua_pushnumber(L, v);
    return 1;
}

// basic window & layout helpers (demo.* API)
static int lua_begin(lua_State* L){ ImGui::Begin(luaL_checkstring(L,1)); return 0; }
static int lua_end  (lua_State* L){ ImGui::End(); return 0; }
static int lua_set_next_window_size(lua_State* L){
    float w=(float)luaL_checknumber(L,1), h=(float)luaL_checknumber(L,2);
    int cond=(int)luaL_optinteger(L,3,ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w,h), cond); return 0;
}
static int lua_begin_table(lua_State* L){
    const char* id = luaL_checkstring(L,1); int cols=(int)luaL_checkinteger(L,2);
    bool ok = ImGui::BeginTable(id, cols, ImGuiTableFlags_SizingStretchSame);
    lua_pushboolean(L, ok); return 1;
}
static int lua_table_next_column(lua_State* L){ ImGui::TableNextColumn(); return 0; }
static int lua_end_table(lua_State* L){ ImGui::EndTable(); return 0; }
static int lua_spacing(lua_State* L){ ImGui::Spacing(); return 0; }
static int lua_separator(lua_State* L){ ImGui::Separator(); return 0; }

// plot + audio
static int lua_plot_sine(lua_State* L){
    double amp = luaL_checknumber(L,1);
    double f   = luaL_checknumber(L,2);
    int    N   = std::clamp((int)luaL_optinteger(L,3,512), 2, 4096);
    static std::vector<float> buf; buf.resize(N);
    const float tp = 6.28318530718f;
    for(int i=0;i<N;++i) buf[i] = (float)(amp * std::sin(tp * f * (float)i / (N-1)));
    ImGui::PlotLines("##sine", buf.data(), N, 0, nullptr, (float)-amp, (float)amp, ImVec2(-1,150));
    return 0;
}
static int lua_audio_set_freq(lua_State* L){ gSine.freq = luaL_checknumber(L,1); return 0; }

// draw torus into FBO and show it centered; side<0 auto-fits, clamped 64..512
static int lua_gl_torus(lua_State* L){
    int   side  = (int)luaL_optinteger(L,1,-1);
    float yaw   = (float)luaL_optnumber (L,2,0.0f);
    float pitch = (float)luaL_optnumber (L,3,0.0f);
    float R     = (float)luaL_optnumber (L,4,0.75f);
    float r     = (float)luaL_optnumber (L,5,0.25f);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (side < 0) side = (int)std::floor(std::max(1.0f, std::min(avail.x, avail.y)));
    side = std::max(64, std::min(side, 512)); // keep window from exploding

    renderTorusInto(gLuaTorusFBO, side, yaw, pitch, R, r);

    ImVec2 start = ImGui::GetCursorPos();
    float offX = std::max(0.0f, (avail.x - (float)side) * 0.5f);
    float offY = std::max(0.0f, (avail.y - (float)side) * 0.5f);
    ImGui::SetCursorPos(ImVec2(start.x + offX, start.y + offY));
    ImGui::Image((ImTextureID)(intptr_t)gLuaTorusFBO.tex,
                 ImVec2((float)side,(float)side),
                 ImVec2(0,1), ImVec2(1,0));
    ImGui::SetCursorPos(start);
    ImGui::Dummy(avail);
    return 0;
}

static void registerAllLua(lua_State* L){
    luaL_Reg fns[] = {
        {"Begin", lua_begin},
        {"End", lua_end},
        {"SetNextWindowSize", lua_set_next_window_size},
        {"BeginTable", lua_begin_table},
        {"TableNextColumn", lua_table_next_column},
        {"EndTable", lua_end_table},
        {"Spacing", lua_spacing},
        {"Separator", lua_separator},
        {"knob_float", lua_knob_float},
        {"plot_sine", lua_plot_sine},
        {"audio_set_freq", lua_audio_set_freq},
        {"gl_torus", lua_gl_torus},
        {nullptr,nullptr}
    };
    luaL_newlib(L, fns);
    lua_setglobal(L, "demo");
}

/*──────────────────── main ───────────────────*/
int main(){
    // audio
    Pa_Initialize();
    PaStream* stream=nullptr;
    Pa_OpenDefaultStream(&stream,0,1,paFloat32,48000,256,paCB,&gSine);
    Pa_StartStream(stream);

    // window + GL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win=glfwCreateWindow(1000,760,"Lua ImGui: Audio + Torus (Knobs)",nullptr,nullptr);
    glfwMakeContextCurrent(win); glfwSwapInterval(1);
    glewExperimental=GL_TRUE; glewInit();

    // ImGui (dark)
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win,true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImVec4 clear = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);

    // Lua
    lua_State* L = luaL_newstate(); luaL_openlibs(L);
    registerAllLua(L);
    if (luaL_dofile(L, "sine_ui.lua") != LUA_OK) {
        std::fprintf(stderr, "[Lua] %s\n", lua_tostring(L,-1));
        lua_pop(L,1);
    }

    // frame loop
    while(!glfwWindowShouldClose(win)){
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        lua_getglobal(L, "draw_ui");
        if(lua_isfunction(L,-1)){
            if(lua_pcall(L,0,0,0)!=LUA_OK){
                std::fprintf(stderr,"[Lua] draw_ui: %s\n", lua_tostring(L,-1));
                lua_pop(L,1);
            }
        } else lua_pop(L,1);

        ImGui::Render();

        int W,H; glfwGetFramebufferSize(win,&W,&H);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,W,H);
        glDisable(GL_DEPTH_TEST);
        glClearColor(clear.x,clear.y,clear.z,clear.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
    }

    Pa_StopStream(stream); Pa_CloseStream(stream); Pa_Terminate();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(); glfwDestroyWindow(win); glfwTerminate();
    return 0;
}
