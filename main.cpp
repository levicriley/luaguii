#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui-knobs.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <portaudio.h>

#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>

// LuaJIT / Lua 5.x headers
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

/*──────────────────── Audio: PortAudio sine ───────────────────*/
struct Sine { double phase=0.0, freq=440.0; } gSine;

static int paCB(const void*, void* out,
                unsigned long frames,
                const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
                void* user) {
    auto* s   = static_cast<Sine*>(user);
    float* buf= static_cast<float*>(out);
    const double sr = 48000.0;
    const double inc= 2.0 * M_PI * s->freq / sr;
    for (unsigned long i=0;i<frames;++i) {
        buf[i] = (float)std::sin(s->phase);
        s->phase += inc;
        if (s->phase > 2*M_PI) s->phase -= 2*M_PI;
    }
    return paContinue;
}

/*──────────────────── Mini FBO ───────────────────*/
struct MiniFBO {
    GLuint fbo=0, tex=0, rbo=0; int w=0, h=0;
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
        glFramebufferTexture2D   (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,       GL_TEXTURE_2D, tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER, rbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

static MiniFBO gLuaTorusFBO;
static GLuint  gTorusProg=0, gTorusVAO=0;
static float   gRainbowSpeed = 0.25f;

/*──────────────────── Torus shader (raymarch rainbow) ───────────────────*/
static const char* kTorusVS = R"(
#version 330
const vec2 V[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
out vec2 uv; void main(){ vec2 p=V[gl_VertexID]; gl_Position=vec4(p,0,1); uv=p*0.5+0.5; }
)";

static const char* kTorusFS = R"(
#version 330
in vec2 uv; out vec4 color;
uniform float yaw, pitch, R, r;
uniform float time, rainbowSpeed;

float sdTorus(vec3 p, vec2 t){ vec2 q=vec2(length(p.xz)-t.x, p.y); return length(q)-t.y; }
mat3 rotY(float a){ float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }
mat3 rotX(float a){ float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }

vec3 nrm(vec3 p, vec2 T){
  float e=.001;
  vec3 n=vec3(
    sdTorus(p+vec3(e,0,0),T)-sdTorus(p-vec3(e,0,0),T),
    sdTorus(p+vec3(0,e,0),T)-sdTorus(p-vec3(0,e,0),T),
    sdTorus(p+vec3(0,0,e),T)-sdTorus(p-vec3(0,0,e),T)
  );
  return normalize(n);
}

vec3 hsv2rgb(vec3 c){
  vec3 p = abs(fract(c.xxx + vec3(0., 2./3., 1./3.)) * 6. - 3.);
  return c.z * mix(vec3(1.), clamp(p - 1., 0., 1.), c.y);
}

void main(){
  vec2 p = uv*2.0 - 1.0;
  vec3 ro = vec3(0,0,3), rd = normalize(vec3(p,-1.5));
  mat3 RY = rotY(yaw), RX = rotX(pitch);

  float t=0.0; bool hit=false; vec2 T=vec2(R,r);
  for(int i=0;i<96;i++){
    vec3 pos = RX*(RY*(ro + rd*t));
    float d = sdTorus(pos, T);
    if (d < 0.001){ hit = true; break; }
    t += d * 0.9;
    if (t > 20.0) break;
  }

  vec3 col=vec3(0.10,0.12,0.16); // bg
  if (hit){
    vec3 pos = RX*(RY*(ro + rd*t));
    float theta = atan(pos.z, pos.x) / (2.0*3.14159265); // ring angle
    vec2  q = vec2(length(pos.xz) - R, pos.y);
    float phi = atan(q.y, q.x) / (2.0*3.14159265);       // tube angle
    float H = fract(theta + phi + time*rainbowSpeed);
    vec3 base = hsv2rgb(vec3(H, 1.0, 1.0));
    vec3 N = nrm(pos, T);
    vec3 L = normalize(vec3(0.5,0.8,0.3));
    float diff = max(dot(N,L),0.0);
    float spec = pow(max(dot(reflect(-L,N), -normalize(RX*(RY*rd))),0.0), 32.0);
    col = base * (0.25 + 0.75*diff) + 0.25*spec;
  }
  color = vec4(col,1.0);
}
)";

static GLuint compile(GLenum t, const char* src){
    GLuint id=glCreateShader(t);
    glShaderSource(id,1,&src,nullptr); glCompileShader(id);
    GLint ok; glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[1024]; glGetShaderInfoLog(id,1024,nullptr,log); std::fprintf(stderr,"%s\n",log); }
    return id;
}

static void ensureTorusProgram(){
    if (gTorusProg) return;
    GLuint vs=compile(GL_VERTEX_SHADER,kTorusVS);
    GLuint fs=compile(GL_FRAGMENT_SHADER,kTorusFS);
    gTorusProg=glCreateProgram();
    glAttachShader(gTorusProg,vs); glAttachShader(gTorusProg,fs);
    glLinkProgram(gTorusProg);
    glDeleteShader(vs); glDeleteShader(fs);
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
    glUniform1f(glGetUniformLocation(gTorusProg,"yaw"),   yaw);
    glUniform1f(glGetUniformLocation(gTorusProg,"pitch"), pitch);
    glUniform1f(glGetUniformLocation(gTorusProg,"R"),     R);
    glUniform1f(glGetUniformLocation(gTorusProg,"r"),     r);
    glUniform1f(glGetUniformLocation(gTorusProg,"time"),  (float)glfwGetTime());
    glUniform1f(glGetUniformLocation(gTorusProg,"rainbowSpeed"), gRainbowSpeed);

    glBindVertexArray(gTorusVAO);
    glDrawArrays(GL_TRIANGLES,0,3);

    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

/*──────────── Lua helpers & bindings ───────────*/
static bool lua_isinteger_compat(lua_State* L, int idx) {
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 503
    return lua_isinteger(L, idx) != 0;
#else
    if (!lua_isnumber(L, idx)) return false;
    lua_Number n = lua_tonumber(L, idx);
    lua_Number i = (lua_Number)(long long)n;
    return fabs(n - i) < 1e-9;
#endif
}

static std::string lower(const char* s){
    std::string r = s ? s : "";
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return r;
}

// only variants present in your imgui-knobs:
static ImGuiKnobVariant knob_variant_from_lua(lua_State* L, int idx) {
    if (lua_isinteger_compat(L, idx)) return (ImGuiKnobVariant)(int)lua_tointeger(L, idx);
    std::string v = lower(luaL_optstring(L, idx, "Tick"));
    if (v=="tick")       return ImGuiKnobVariant_Tick;
    if (v=="dot")        return ImGuiKnobVariant_Dot;
    if (v=="wiper")      return ImGuiKnobVariant_Wiper;
    if (v=="wiperonly" || v=="wiper_only") return ImGuiKnobVariant_WiperOnly;
    if (v=="wiperdot"  || v=="wiper_dot")  return ImGuiKnobVariant_WiperDot;
    if (v=="stepped")    return ImGuiKnobVariant_Stepped;
    if (v=="space")      return ImGuiKnobVariant_Space;
    return ImGuiKnobVariant_Tick;
}

// knob_float_full(label, val, min, max [, speed [, format [, variant [, size [, flags [, steps [, angle_min [, angle_max ]]]]]]]])
static int lua_knob_float_full(lua_State* L){
    const char* label = luaL_checkstring(L, 1);
    float v          = (float)luaL_checknumber(L, 2);
    float vmin       = (float)luaL_checknumber(L, 3);
    float vmax       = (float)luaL_checknumber(L, 4);
    float speed      = (float)luaL_optnumber (L, 5, 0.01f);
    const char* fmt  = luaL_optstring(L, 6, "%.3f");
    ImGuiKnobVariant variant = knob_variant_from_lua(L, 7);
    float size       = (float)luaL_optnumber (L, 8, 0.0f);
    int flags        = (int)  luaL_optinteger(L, 9, 0);
    int steps        = (int)  luaL_optinteger(L,10, 0);
    float angle_min  = (float)luaL_optnumber (L,11, 0.0f);
    float angle_max  = (float)luaL_optnumber (L,12, 6.28318530718f);

    ImGuiKnobs::Knob(label, &v, vmin, vmax, speed, fmt, variant, size, (ImGuiKnobFlags)flags, steps, angle_min, angle_max);
    lua_pushnumber(L, v);
    return 1;
}

// window/layout helpers
static int lua_begin(lua_State* L){ ImGui::Begin(luaL_checkstring(L,1)); return 0; }
static int lua_end  (lua_State* L){ ImGui::End(); return 0; }
static int lua_set_next_window_size(lua_State* L){
    float w=(float)luaL_checknumber(L,1), h=(float)luaL_checknumber(L,2);
    int cond=(int)luaL_optinteger(L,3,ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w,h), cond); return 0;
}
static int lua_separator(lua_State* L){ ImGui::Separator(); return 0; }
static int lua_spacing(lua_State* L){ ImGui::Spacing(); return 0; }
static int lua_begin_table(lua_State* L){
    const char* id = luaL_checkstring(L,1); int cols=(int)luaL_checkinteger(L,2);
    bool ok = ImGui::BeginTable(id, cols, ImGuiTableFlags_SizingStretchSame);
    lua_pushboolean(L, ok); return 1;
}
static int lua_table_next_column(lua_State* L){ ImGui::TableNextColumn(); return 0; }
static int lua_end_table(lua_State* L){ ImGui::EndTable(); return 0; }
static int lua_button(lua_State* L){ bool pressed=ImGui::Button(luaL_checkstring(L,1)); lua_pushboolean(L,pressed); return 1; }

// sine plot
static int lua_plot_sine(lua_State* L){
    double amp = luaL_checknumber(L,1);
    double f   = luaL_checknumber(L,2);
    int    N   = std::clamp((int)luaL_optinteger(L,3,512), 2, 4096);
    static std::vector<float> buf; buf.resize(N);
    const float tp = 6.28318530718f;
    for (int i=0;i<N;++i) buf[i] = (float)(amp * std::sin(tp * f * (float)i / (N-1)));
    ImGui::PlotLines("##sine", buf.data(), N, 0, nullptr, (float)-amp, (float)amp, ImVec2(-1,120));
    return 0;
}

// audio controls
static int lua_audio_set_freq(lua_State* L){ gSine.freq = luaL_checknumber(L,1); return 0; }

// rainbow speed
static int lua_gl_torus_rainbow_speed(lua_State* L){ gRainbowSpeed = (float)luaL_checknumber(L,1); return 0; }

// draw torus into FBO and show it centered; side<0 => auto-fit (clamped 96..512)
static int lua_gl_torus(lua_State* L){
    int   side  = (int)luaL_optinteger(L,1,-1);
    float yaw   = (float)luaL_optnumber (L,2,0.0f);
    float pitch = (float)luaL_optnumber (L,3,0.0f);
    float R     = (float)luaL_optnumber (L,4,0.75f);
    float r     = (float)luaL_optnumber (L,5,0.25f);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (side < 0) side = (int)std::floor(std::max(1.0f, std::min(avail.x, avail.y)));
    side = std::max(96, std::min(side, 512));

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

static void push_knob_enums(lua_State* L){
    lua_newtable(L);
    #define KV(name) lua_pushinteger(L, (int)ImGuiKnobVariant_##name); lua_setfield(L, -2, #name)
    KV(Tick); KV(Dot); KV(Wiper); KV(WiperOnly); KV(WiperDot); KV(Stepped); KV(Space);
    #undef KV
    lua_setglobal(L, "KnobVariant");
}

static void registerAllLua(lua_State* L){
    luaL_Reg fns[] = {
        {"Begin", lua_begin}, {"End", lua_end},
        {"SetNextWindowSize", lua_set_next_window_size},
        {"Separator", lua_separator}, {"Spacing", lua_spacing},
        {"BeginTable", lua_begin_table}, {"TableNextColumn", lua_table_next_column}, {"EndTable", lua_end_table},
        {"Button", lua_button},
        {"knob_float_full", lua_knob_float_full},
        {"plot_sine", lua_plot_sine},
        {"audio_set_freq", lua_audio_set_freq},
        {"gl_torus", lua_gl_torus}, {"gl_torus_rainbow_speed", lua_gl_torus_rainbow_speed},
        {nullptr,nullptr}
    };
    luaL_newlib(L, fns);
    lua_setglobal(L, "demo");
    push_knob_enums(L);
}

/*──────────────────── main ───────────────────*/
int main(){
    // Audio init
    Pa_Initialize();
    PaStream* stream=nullptr;
    Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, 48000, 256, paCB, &gSine);
    Pa_StartStream(stream);

    // Window + GL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win=glfwCreateWindow(1000,760,"Lua ImGui: Audio + Rainbow Torus (Knobs)",nullptr,nullptr);
    glfwMakeContextCurrent(win); glfwSwapInterval(1);
    glewExperimental=GL_TRUE; glewInit();

    // ImGui
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win,true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImVec4 clear = ImVec4(0.16f,0.18f,0.22f,1.0f);

    // Lua
    lua_State* L = luaL_newstate(); luaL_openlibs(L);
    registerAllLua(L);
    if (luaL_dofile(L, "sine_ui.lua") != LUA_OK) {
        std::fprintf(stderr, "[Lua] %s\n", lua_tostring(L,-1));
        lua_pop(L,1);
    }

    while(!glfwWindowShouldClose(win)){
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        lua_getglobal(L, "draw_ui");
        if (lua_isfunction(L,-1)) {
            if (lua_pcall(L,0,0,0)!=LUA_OK){ std::fprintf(stderr,"[Lua] draw_ui: %s\n", lua_tostring(L,-1)); lua_pop(L,1); }
        } else { lua_pop(L,1); }

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

    // shutdown
    Pa_StopStream(stream); Pa_CloseStream(stream); Pa_Terminate();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(); glfwDestroyWindow(win); glfwTerminate();
    return 0;
}
