#include "cube.hpp"
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

namespace cube {

// XXX just for now
namespace world{
extern int raycast;
extern void castrayprout(float fovy, float aspect, float farplane);
}
namespace rr { extern int curvert; }
namespace ogl {

int xtraverts = 0;

/*-------------------------------------------------------------------------
 - very simple state tracking
 -------------------------------------------------------------------------*/
union {
  struct {
    u32 shader:1; // will force to reload everything
    u32 mvp:1;
    u32 fog:1;
    u32 overbright:1;
  } flags;
  uint any;
} dirty;
static u32 bindedvbo[BUFFER_NUM];
static u32 enabledattribarray[ATTRIB_NUM];
static struct shader *bindedshader = NULL;

static const u32 TEX_NUM = 8;
static u32 bindedtexture[TEX_NUM];

void enableattribarray(u32 target) {
  if (!enabledattribarray[target]) {
    enabledattribarray[target] = 1;
    OGL(EnableVertexAttribArray, target);
  }
}

void disableattribarray(u32 target) {
  if (enabledattribarray[target]) {
    enabledattribarray[target] = 0;
    OGL(DisableVertexAttribArray, target);
  }
}

/*--------------------------------------------------------------------------
 - immediate mode and buffer support
 -------------------------------------------------------------------------*/
static const u32 glbufferbinding[BUFFER_NUM] = {
  GL_ARRAY_BUFFER,
  GL_ELEMENT_ARRAY_BUFFER
};
void bindbuffer(u32 target, u32 buffer) {
  if (bindedvbo[target] != buffer) {
    OGL(BindBuffer, glbufferbinding[target], buffer);
    bindedvbo[target] = buffer;
  }
}

// attributes in the vertex buffer
static struct {int n, type, offset;} immattribs[ATTRIB_NUM];
static int immvertexsz = 0;

// we use two big circular buffers to handle immediate mode
static int immbuffersize = 16*MB;
static int bigvbooffset=0, bigibooffset=0;
static int drawibooffset=0, drawvbooffset=0;
static GLuint bigvbo=0u, bigibo=0u;

static void initbuffer(GLuint &bo, int target, int size) {
  if (bo == 0u) OGL(GenBuffers, 1, &bo);
  bindbuffer(target, bo);
  OGL(BufferData, glbufferbinding[target], size, NULL, GL_DYNAMIC_DRAW);
  bindbuffer(target, 0);
}

static void imminit(void) {
  initbuffer(bigvbo, ARRAY_BUFFER, immbuffersize);
  initbuffer(bigibo, ELEMENT_ARRAY_BUFFER, immbuffersize);
  memset(immattribs, 0, sizeof(immattribs));
}

void immattrib(int attrib, int n, int type, int offset) {
  immattribs[attrib].n = n;
  immattribs[attrib].type = type;
  immattribs[attrib].offset = offset;
}

void immvertexsize(int sz) { immvertexsz = sz; }

static void immsetallattribs(void) {
  loopi(ATTRIB_NUM) {
    if (!enabledattribarray[i]) continue;
    const void *fake = (const void *) intptr_t(drawvbooffset+immattribs[i].offset);
    OGL(VertexAttribPointer, i, immattribs[i].n, immattribs[i].type, 0, immvertexsz, fake);
  }
}

static bool immsetdata(int target, int sz, const void *data) {
  if (sz >= immbuffersize) {
    console::out("too many immediate items to render");
    return false;
  }
  GLuint &bo = target==ARRAY_BUFFER ? bigvbo : bigibo;
  int &offset = target==ARRAY_BUFFER ? bigvbooffset : bigibooffset;
  int &drawoffset = target==ARRAY_BUFFER ? drawvbooffset : drawibooffset;
  if (offset+sz > immbuffersize) {
    OGL(Flush);
    bindbuffer(target, 0);
    offset = 0u;
    bindbuffer(target, bo);
  }
  bindbuffer(target, bo);
  OGL(BufferSubData, glbufferbinding[target], offset, sz, data);
  drawoffset = offset;
  offset += sz;
  return true;
}

static bool immvertices(int sz, const void *vertices) {
  return immsetdata(ARRAY_BUFFER, sz, vertices);
}

void immdrawarrays(int mode, int count, int type) {
  drawarrays(mode,count,type);
}

void immdrawelements(int mode, int count, int type, const void *indices, const void *vertices) {
  int indexsz = count;
  int maxindex = 0;
  switch (type) {
    case GL_UNSIGNED_INT:
      indexsz*=sizeof(u32);
      loopi(count) maxindex = max(maxindex,int(((u32*)indices)[i]));
    break;
    case GL_UNSIGNED_SHORT:
      indexsz*=sizeof(u16);
      loopi(count) maxindex = max(maxindex,int(((u16*)indices)[i]));
    break;
    case GL_UNSIGNED_BYTE:
      indexsz*=sizeof(u8);
      loopi(count) maxindex = max(maxindex,int(((u8*)indices)[i]));
    break;
  };
  if (!immsetdata(ELEMENT_ARRAY_BUFFER, indexsz, indices)) return;
  if (!immvertices((maxindex+1)*immvertexsz, vertices)) return;
  immsetallattribs();
  const void *fake = (const void *) intptr_t(drawibooffset);
  drawelements(mode, count, type, fake);
}

void immdraw(int mode, int pos, int tex, int col, size_t n, const float *data) {
  const int sz = (pos+tex+col)*sizeof(float);
  if (!immvertices(n*sz, data)) return;
  loopi(ATTRIB_NUM) disableattribarray(i);
  if (pos) {
    immattrib(ogl::POS0, pos, GL_FLOAT, (tex+col)*sizeof(float));
    enableattribarray(POS0);
  } else
    disableattribarray(POS0);
  if (tex) {
    immattrib(ogl::TEX0, tex, GL_FLOAT, col*sizeof(float));
    enableattribarray(TEX0);
  } else
    disableattribarray(TEX0);
  if (col) {
    immattrib(ogl::COL, col, GL_FLOAT, 0);
    enableattribarray(COL);
  } else
    disableattribarray(COL);
  immvertexsize(sz);
  immsetallattribs();
  immdrawarrays(mode, 0, n);
}

/*-------------------------------------------------------------------------
 - matrix handling. very inspired by opengl :-)
 -------------------------------------------------------------------------*/
enum {MATRIX_STACK = 4};
static mat4x4f vp[MATRIX_MODE] = {one, one};
static mat4x4f vpstack[MATRIX_STACK][MATRIX_MODE];
static int vpdepth = 0;
static int vpmode = MODELVIEW;
static mat4x4f viewproj(one);

void matrixmode(int mode) { vpmode = mode; }
const mat4x4f &matrix(int mode) { return vp[mode]; }
void loadmatrix(const mat4x4f &m) {
  dirty.flags.mvp=1;
  vp[vpmode] = m;
}
void identity(void) {
  dirty.flags.mvp=1;
  vp[vpmode] = mat4x4f(one);
}
void translate(const vec3f &v) {
  dirty.flags.mvp=1;
  vp[vpmode] = vp[vpmode]*mat4x4f::translate(v);
}
void mulmatrix(const mat4x4f &m) {
  dirty.flags.mvp=1;
  vp[vpmode] = m*vp[vpmode];
}
void rotate(float angle, const vec3f &axis) {
  dirty.flags.mvp=1;
  vp[vpmode] = vp[vpmode]*mat4x4f::rotate(angle,axis);
}
void perspective(float fovy, float aspect, float znear, float zfar) {
  dirty.flags.mvp=1;
  vp[vpmode] = vp[vpmode]*cube::perspective(fovy,aspect,znear,zfar);
}
void ortho(float left, float right, float bottom, float top, float znear, float zfar) {
  dirty.flags.mvp=1;
  vp[vpmode] = vp[vpmode]*cube::ortho(left,right,bottom,top,znear,zfar);
}
void scale(const vec3f &s) {
  dirty.flags.mvp=1;
  vp[vpmode] = scale(vp[vpmode],s);
}
void pushmatrix(void) {
  assert(vpdepth+1<MATRIX_STACK);
  vpstack[vpdepth++][vpmode] = vp[vpmode];
}
void popmatrix(void) {
  assert(vpdepth>0);
  dirty.flags.mvp=1;
  vp[vpmode] = vpstack[--vpdepth][vpmode];
}

/*--------------------------------------------------------------------------
 - management of texture slots each texture slot can have multiple texture
 - frames, of which currently only the first is used additional frames can be
 - used for various shaders
 -------------------------------------------------------------------------*/
static const int MAXTEX = 1000;
static const int FIRSTTEX = 1000; // opengl id = loaded id + FIRSTTEX
static const int MAXFRAMES = 2; // increase for more complex shader defs
static int texx[MAXTEX]; // (loaded texture) -> (name, size)
static int texy[MAXTEX];
static string texname[MAXTEX];
static int curtex = 0;
static int glmaxtexsize = 256;
static int curtexnum = 0;

// std 1+, sky 14+, mdls 20+
static int mapping[256][MAXFRAMES]; // (texture, frame) -> (oglid, name)
static string mapname[256][MAXFRAMES];

static void purgetextures(void) {loopi(256)loop(j,MAXFRAMES)mapping[i][j]=0;}

static const u32 IDNUM = 2*MAXTEX;
static u32 generatedids[IDNUM];

static void bindtexture(u32 target, u32 texslot, u32 id) {
  if (bindedtexture[texslot] == id) return;
  bindedtexture[texslot] = id;
  if (id >= IDNUM) fatal("out of bound texture ID");
  OGL(ActiveTexture, GL_TEXTURE0 + texslot);
  OGL(BindTexture, target, id);
}

void bindgametexture(u32 target, u32 id) {
  bindtexture(GL_TEXTURE_2D, 0, generatedids[id]);
}

INLINE bool ispoweroftwo(unsigned int x) { return ((x & (x - 1)) == 0); }

bool installtex(int tnum, const char *texname, int &xs, int &ys, bool clamp) {
  SDL_Surface *s = IMG_Load(texname);
  if (!s) {
    console::out("couldn't load texture %s", texname);
    return false;
  }
#if !defined(EMSCRIPTEN)
  else if (s->format->BitsPerPixel!=24) {
    console::out("texture must be 24bpp: %s (got %i bpp)", texname, s->format->BitsPerPixel);
    return false;
  }
#endif

  if (tnum >= int(IDNUM)) fatal("out of bound texture ID");
  if (generatedids[tnum] == 0u)
    OGL(GenTextures, 1, generatedids + tnum);
  loopi(int(TEX_NUM)) bindedtexture[i] = 0;
  console::out("loading %s (%ix%i)", texname, s->w, s->h);
  ogl::bindgametexture(GL_TEXTURE_2D, tnum);
  OGL(PixelStorei, GL_UNPACK_ALIGNMENT, 1);
  xs = s->w;
  ys = s->h;
  if (xs>glmaxtexsize || ys>glmaxtexsize)
    fatal("texture dimensions are too large");
  if (s->format->BitsPerPixel == 24)
    OGL(TexImage2D, GL_TEXTURE_2D, 0, GL_RGB, xs, ys, 0, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  else if (s->format->BitsPerPixel == 32)
    OGL(TexImage2D, GL_TEXTURE_2D, 0, GL_RGBA, xs, ys, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->pixels);
  else
    fatal("unsupported texture format");

  if (ispoweroftwo(xs) && ispoweroftwo(ys)) {
    OGL(GenerateMipmap, GL_TEXTURE_2D);
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  } else {
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }
  SDL_FreeSurface(s);
  return true;
}

int lookuptex(int tex, int &xs, int &ys) {
  const int frame = 0; // other frames?
  int tid = mapping[tex][frame];

  if (tid>=FIRSTTEX) {
    xs = texx[tid-FIRSTTEX];
    ys = texy[tid-FIRSTTEX];
    return tid;
  }

  xs = ys = 16;
  if (!tid) return 1; // crosshair :)

  loopi(curtex) // lazily happens once per "texture" command
    if (strcmp(mapname[tex][frame], texname[i])==0) {
      mapping[tex][frame] = tid = i+FIRSTTEX;
      xs = texx[i];
      ys = texy[i];
      return tid;
    }

  if (curtex==MAXTEX) fatal("loaded too many textures");

  const int tnum = curtex+FIRSTTEX;
  strcpy_s(texname[curtex], mapname[tex][frame]);

  sprintf_sd(name)("packages%c%s", PATHDIV, texname[curtex]);

  if (installtex(tnum, name, xs, ys)) {
    mapping[tex][frame] = tnum;
    texx[curtex] = xs;
    texy[curtex] = ys;
    curtex++;
    return tnum;
  } else
    return mapping[tex][frame] = FIRSTTEX;  // temp fix
}

static void texturereset(void) { curtexnum = 0; }
COMMAND(texturereset, ARG_NONE);

static void texture(const char *aframe, const char *name) {
  const int num = curtexnum++, frame = atoi(aframe);
  if (num<0 || num>=256 || frame<0 || frame>=MAXFRAMES) return;
  mapping[num][frame] = 1;
  char *n = mapname[num][frame];
  strcpy_s(n, name);
  path(n);
}
COMMAND(texture, ARG_2STR);

/*--------------------------------------------------------------------------
 - sphere management
 -------------------------------------------------------------------------*/
static GLuint spherevbo = 0;
static int spherevertn = 0;

static void buildsphere(float radius, int slices, int stacks) {
  vector<arrayf<5>> v;
  loopj(stacks) {
    const float angle0 = M_PI * float(j) / float(stacks);
    const float angle1 = M_PI * float(j+1) / float(stacks);
    const float zLow = radius * cosf(angle0);
    const float zHigh = radius * cosf(angle1);
    const float sin1 = radius * sinf(angle0);
    const float sin2 = radius * sinf(angle1);

    loopi(slices+1) {
      const float angle = 2.f * M_PI * float(i) / float(slices);
      const float sin0 = sinf(angle);
      const float cos0 = cosf(angle);
      const int start = (i==0&&j!=0)?2:1;
      const int end = (i==slices&&j!=stacks-1)?2:1;
      loopk(start) { // stick the strips together
        const float s = 1.f-float(i)/slices, t = 1.f-float(j)/stacks;
        const float x = sin1*sin0, y = sin1*cos0, z = zLow;
        v.add(arrayf<5>(s, t, x, y, z));
      }
      loopk(end) { // idem
        const float s = 1.f-float(i)/slices, t = 1.f-float(j+1)/stacks;
        const float x = sin2*sin0, y = sin2*cos0, z = zHigh;
        v.add(arrayf<5>(s, t, x, y, z));
      }
      spherevertn += start+end;
    }
  }

  const size_t sz = sizeof(arrayf<5>) * v.length();
  OGL(GenBuffers, 1, &spherevbo);
  ogl::bindbuffer(ARRAY_BUFFER, spherevbo);
  OGL(BufferData, GL_ARRAY_BUFFER, sz, &v[0][0], GL_STATIC_DRAW);
}

/*--------------------------------------------------------------------------
 - overbright -> just multiplies final color
 -------------------------------------------------------------------------*/
static float overbrightf = 1.f;
static void overbright(float amount) {
  dirty.flags.overbright=1;
  overbrightf = amount;
}

/*--------------------------------------------------------------------------
 - quick and dirty shader management
 -------------------------------------------------------------------------*/
static bool checkshader(GLuint shadername) {
  GLint result = GL_FALSE;
  int infologlength;

  if (!shadername) return false;
  OGL(GetShaderiv, shadername, GL_COMPILE_STATUS, &result);
  OGL(GetShaderiv, shadername, GL_INFO_LOG_LENGTH, &infologlength);
  if (infologlength) {
    char *buffer = new char[infologlength + 1];
    buffer[infologlength] = 0;
    OGL(GetShaderInfoLog, shadername, infologlength, NULL, buffer);
    printf("%s",buffer);
    delete [] buffer;
  }
  if (result == GL_FALSE) fatal("OGL: failed to compile shader");
  return result == GL_TRUE;
}

static GLuint loadshader(GLenum type, const char *source, const char *rulestr) {
  GLuint name;
  OGLR(name, CreateShader, type);
  const char *sources[] = {rulestr, source};
  OGL(ShaderSource, name, 2, sources, NULL);
  OGL(CompileShader, name);
  if (!checkshader(name)) fatal("OGL: shader not valid");
  return name;
}

static GLuint loadprogram(const char *vertstr, const char *fragstr, uint rules) {
  GLuint program = 0;
  sprintf_sd(rulestr)(IF_NOT_EMSCRIPTEN("#version 130\n")
                      IF_EMSCRIPTEN("precision highp float;\n")
                      "#define USE_FOG %i\n"
                      "#define USE_KEYFRAME %i\n"
                      "#define USE_DIFFUSETEX %i\n",
                      rules&FOG,rules&KEYFRAME,rules&DIFFUSETEX);
  const GLuint vert = loadshader(GL_VERTEX_SHADER, vertstr, rulestr);
  const GLuint frag = loadshader(GL_FRAGMENT_SHADER, fragstr, rulestr);
  OGLR(program, CreateProgram);
  OGL(AttachShader, program, vert);
  OGL(AttachShader, program, frag);
  OGL(DeleteShader, vert);
  OGL(DeleteShader, frag);
  return program;
}

#if defined(EMSCRIPTEN)
#define VS_IN "attribute"
#define VS_OUT "varying"
#define PS_IN "varying"
#else
#define VS_IN "in"
#define VS_OUT "out"
#define PS_IN "in"
#endif // EMSCRIPTEN

static const char ubervert[] = {
  "uniform mat4 u_mvp;\n"
  "#if USE_FOG\n"
  "  uniform vec4 u_zaxis;\n"
  "  " VS_OUT " float fs_fogz;\n"
  "#endif\n"
  "#if USE_KEYFRAME\n"
  "  uniform float u_delta;\n"
  "  " VS_IN " vec3 vs_pos0, vs_pos1;\n"
  "#else\n"
  "  " VS_IN " vec3 vs_pos;\n"
  "#endif\n"
  VS_IN " vec4 vs_col;\n"
  "#if USE_DIFFUSETEX\n"
  "  " VS_IN " vec2 vs_tex;\n"
  "  " VS_OUT " vec2 fs_tex;\n"
  "#endif\n"
  VS_OUT " vec4 fs_col;\n"
  "void main() {\n"
  "#if USE_DIFFUSETEX\n"
  "  fs_tex = vs_tex;\n"
  "#endif\n"
  "  fs_col = vs_col;\n"
  "#if USE_KEYFRAME\n"
  "  vec3 vs_pos = mix(vs_pos0,vs_pos1,u_delta);\n"
  "#endif\n"
  "#if USE_FOG\n"
  "  fs_fogz = dot(u_zaxis.xyz,vs_pos)+u_zaxis.w;\n"
  "#endif\n"
  "  gl_Position = u_mvp*vec4(vs_pos,1.0);\n"
  "}\n"
};
static const char uberfrag[] = {
  "#if USE_DIFFUSETEX\n"
  "  uniform sampler2D u_diffuse;\n"
  "  " PS_IN " vec2 fs_tex;\n"
  "#endif\n"
  "#if USE_FOG\n"
  "  uniform vec4 u_fogcolor;\n"
  "  uniform vec2 u_fogstartend;\n"
  "  " PS_IN " float fs_fogz;\n"
  "#endif\n"
  "uniform float u_overbright;\n"
  PS_IN " vec4 fs_col;\n"
  IF_NOT_EMSCRIPTEN("out vec4 rt_c;\n")
  "void main() {\n"
  "  vec4 col;\n"
  "#if USE_DIFFUSETEX\n"
  "  col = texture2D(u_diffuse, fs_tex);\n"
  "  col *= fs_col;\n"
  "#else\n"
  "  col = fs_col;\n"
  "#endif\n"
  "#if USE_FOG\n"
  "  float factor = clamp((-fs_fogz-u_fogstartend.x)*u_fogstartend.y,0.0,1.0)\n;"
  "  col.xyz = mix(col.xyz,u_fogcolor.xyz,factor);\n"
  "#endif\n"
  "  col.xyz *= u_overbright;\n"
  IF_NOT_EMSCRIPTEN("  rt_c = col;\n")
  IF_EMSCRIPTEN("  gl_FragColor = col;\n")
  "}\n"
};

static struct shader {
  uint rules; // fog,keyframe...?
  GLuint program; // ogl program
  GLuint u_diffuse, u_delta, u_mvp, u_overbright; // uniforms
  GLuint u_zaxis, u_fogstartend, u_fogcolor; // uniforms
} shaders[shadern];

static const char watervert[] = { // use DIFFUSETEX
  "#define PI 3.14159265\n"
  "uniform mat4 u_mvp;\n"
  "uniform vec2 u_duv, u_dxy;\n"
  "uniform float u_hf, u_delta;\n"
  VS_IN " vec2 vs_pos, vs_tex;\n"
  VS_IN " vec4 vs_col;\n"
  VS_OUT " vec2 fs_tex;\n"
  VS_OUT " vec4 fs_col;\n"
  "float dx(float x) { return x+sin(x*2.0+u_delta/1000.0)*0.04; }\n"
  "float dy(float x) { return x+sin(x*2.0+u_delta/900.0+PI/5.0)*0.05; }\n"
  "void main() {\n"
  "  fs_tex = vec2(dx(vs_tex.x+u_duv.x),dy(vs_tex.y+u_duv.y));\n"
  "  vec2 absp = u_dxy+vs_pos;\n"
  "  vec3 pos = vec3(absp.x,u_hf-sin(absp.x*absp.y*0.1+u_delta/300.0)*0.2,absp.y);\n"
  "  fs_col = vs_col;\n"
  "  gl_Position = u_mvp * vec4(pos,1.0);\n"
  "}\n"
};
static struct watershader : shader {
  GLuint u_duv, u_dxy, u_hf;
} watershader;

static vec4f fogcolor;
static vec2f fogstartend;

static void bindshader(shader &shader) {
  if (bindedshader != &shader) {
    bindedshader = &shader;
    dirty.any = ~0x0;
    OGL(UseProgram, bindedshader->program);
  }
}

void bindshader(uint flags) { bindshader(shaders[flags]); }

static void linkshader(shader &shader) {
  OGL(LinkProgram, shader.program);
  OGL(ValidateProgram, shader.program);
}

static void setshaderuniform(shader &shader) {
  OGL(UseProgram, shader.program);
  OGLR(shader.u_overbright, GetUniformLocation, shader.program, "u_overbright");
  OGLR(shader.u_mvp, GetUniformLocation, shader.program, "u_mvp");
  if (shader.rules&KEYFRAME)
    OGLR(shader.u_delta, GetUniformLocation, shader.program, "u_delta");
  else
    shader.u_delta = 0;
  if (shader.rules&DIFFUSETEX) {
    OGLR(shader.u_diffuse, GetUniformLocation, shader.program, "u_diffuse");
    OGL(Uniform1i, shader.u_diffuse, 0);
  }
  if (shader.rules&FOG) {
    OGLR(shader.u_zaxis, GetUniformLocation, shader.program, "u_zaxis");
    OGLR(shader.u_fogcolor, GetUniformLocation, shader.program, "u_fogcolor");
    OGLR(shader.u_fogstartend, GetUniformLocation, shader.program, "fogstartend");
  }
  OGL(UseProgram, 0);
}

static void compileshader(shader &shader, const char *vert, const char *frag, uint rules) {
  memset(&shader, 0, sizeof(struct shader));
  shader.program = loadprogram(vert, frag, rules);
  shader.rules = rules;
  if (shader.rules&KEYFRAME) {
    OGL(BindAttribLocation, shader.program, POS0, "vs_pos0");
    OGL(BindAttribLocation, shader.program, POS1, "vs_pos1");
  } else
    OGL(BindAttribLocation, shader.program, POS0, "vs_pos");
  if (shader.rules&DIFFUSETEX)
    OGL(BindAttribLocation, shader.program, TEX0, "vs_tex");
  OGL(BindAttribLocation, shader.program, COL, "vs_col");
#if !defined(EMSCRIPTEN)
  OGL(BindFragDataLocation, shader.program, 0, "rt_col");
#endif // EMSCRIPTEN
}

static void buildshader(shader &shader, const char *vert, const char *frag, uint rules) {
  compileshader(shader, vert, frag, rules);
  linkshader(shader);
  setshaderuniform(shader);
}

static void buildubershader(shader &shader, uint rules) {
  buildshader(shader, ubervert, uberfrag, rules);
}

VAR(forcebuild, 0, 0, 1);

/*--------------------------------------------------------------------------
 - over-simple sun shadow
 -------------------------------------------------------------------------*/
static vec3f ldir;
VAR(sampling, 0,0,1);
static vec3f computecolor(vec3f pos, int face) {
#if 1
  const auto n = 0.01f*vec3f(cubenorms[face]);
  const mat3x3f m(n);
  float l = 0.f;
  loopi(8) loopj(8) {
    const float rd = 0.5f*sqrt(float(i)*0.125f);
    const float theta = 2*PI*float(j)*0.125f;
    vec3f u,v;
    if (sampling) {
      u = rd*cos(theta)*m.vx;
      v = rd*sin(theta)*m.vz;
    } else {
      u = (i*0.125f-0.5f)*m.vx;
      v = (j*0.125f-0.5f)*m.vz;
    }
    //const vec3f u = (i*0.125f-0.5f)*m.vx;
    //const vec3f v = (j*0.125f-0.5f)*m.vz;
    const ray r(pos+n+u+v, ldir);
    const auto res = world::castray(r);
    l += (res.isec ? 0.0f : 1.f) *max(dot(ldir,normalize(n)),0.f);
  }
  return vec3f(l) / 64.f;
#endif
}

/*--------------------------------------------------------------------------
 - surface parameterization per brick
 -------------------------------------------------------------------------*/
VAR(lmres, 2, 4, 32);

static const int maxlmw = 256;

// static const vec2i lmindexdim(world::lvl1*world::lvl1, world::lvl1*8);
static const vec2i lmindexdim(256);

INLINE vec2i getlmuv(vec3i idx, u32 face) {
  const int chan = face/2;
  const vec3i lmidx = chan==0?idx.yzx():(chan==1?idx.xzy():idx);
  return vec2i(lmidx.x+world::lvl1*lmidx.z, lmidx.y+world::lvl1*face);
  //return vec2i(lmidx.x, lmidx.y);
}

struct surfaceparamctx {
  INLINE surfaceparamctx(const world::lvl1grid &b) :
    b(b), face(0), lmdim(zero), lm(NULL),
    lmindex(lmindexdim.x*lmindexdim.y) {
    loopv(lmindex) lmindex[i] = vec4<u8>(zero);
  }
  INLINE u32 getindex(vec3i idx) {
    const vec2i uv = getlmuv(idx, face);
    return uv.x+uv.y*lmindexdim.x;
  }
  const world::lvl1grid &b;
  u32 face;
  vec2<u16> lmdim;
  u32 *lm;
  vector<vec4<u8>> lmindex;
};

INLINE bool visibleface(vec3i xyz, u32 face) {
  return world::getcube(xyz).mat != world::EMPTY &&
         world::getcube(xyz+cubenorms[face]).mat == world::EMPTY;
}
static void buildlmindex(surfaceparamctx &ctx, vec3i xyz, vec3i idx) {
  if (!visibleface(xyz, ctx.face)) return;

  idx += ctx.face%2 ? cubenorms[ctx.face] : zero;
  const vec2i uv0 = getlmuv(idx, ctx.face);
  //console::out("uv0 [%i %i]", uv0.x, uv0.y);

  const s32 index = ctx.getindex(idx);
  ctx.lmindex[index] = vec4<u8>(ctx.lmdim.x, ctx.lmdim.y, 0, 0);
#if 0
  console::out("[%i %i %i %i]",
      ctx.lmindex[index].x,
      ctx.lmindex[index].y,
      ctx.lmindex[index].z,
      ctx.lmindex[index].w);
#endif
  ctx.lmdim.x += lmres;
  if (ctx.lmdim.x >= maxlmw) {
    ctx.lmdim.x = 0;
    ctx.lmdim.y += lmres;
  }
}

static void buildlmdata(surfaceparamctx &ctx, vec3i xyz, vec3i idx) {
  if (!visibleface(xyz, ctx.face)) return;

  // try a chessboard pattern first
  idx += ctx.face%2 ? cubenorms[ctx.face] : zero;
  const int chan = ctx.face/2;
  const vec2i uv = chan==0?idx.yz():(chan==1?idx.xz():idx.xy());
#if 1
  // update the lightmap with chessboard color
  const auto index = ctx.getindex(idx);
  const auto lmuv = ctx.lmindex[index];

  assert(all(vec2i(lmuv.xy())+vec2i(lmres) <= vec2i(ctx.lmdim)));
  u32 *l = ctx.lm + lmuv.y*ctx.lmdim.x + lmuv.x;
  loopi(lmres) loopj(lmres) {
    const u32 pattern = ((uv.x&1)^(uv.y&1))^((i&1)^(j&1));
    const u32 color = pattern ? ~0x0u : 0x0u;
    l[i*ctx.lmdim.x+j] = color;
  }
#endif
}

static void buildlightmap(world::lvl1grid &b, const vec3i &org) {
  if (b.dirty==0 && !forcebuild) return;
  b.dirty = 0;
  surfaceparamctx ctx(b);
  loopi(6) {
    ctx.face = i;
    loopxyz(0, b.size(), buildlmindex(ctx, org+xyz, xyz));
  }
#if 0
  ctx.lmdim.x = maxlmw;
  ctx.lmdim.y += lmres;
#else
  assert(ctx.lmdim.y <= 256);
  ctx.lmdim.x = 256;
  ctx.lmdim.y = 256;
#endif
  const u32 lmn = ctx.lmdim.x*ctx.lmdim.y;
  ctx.lm = new u32[lmn];
  memset(ctx.lm, 0, sizeof(u32)*lmn);
  loopi(lmn) ctx.lm[i] = 0;
  loopi(6) {
    ctx.face = i;
    loopxyz(0, b.size(), buildlmdata(ctx, org+xyz, xyz));
  }

  // build light map index texture
  OGL(GenTextures, 1, &b.lmindex);
  ogl::bindtexture(GL_TEXTURE_2D, 0, b.lmindex);
  OGL(PixelStorei, GL_UNPACK_ALIGNMENT, 1);
  OGL(TexImage2D, GL_TEXTURE_2D, 0, GL_RGBA, lmindexdim.x, lmindexdim.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &ctx.lmindex[0]);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // build light map texture
  OGL(GenTextures, 1, &b.lm);
  ogl::bindtexture(GL_TEXTURE_2D, 0, b.lm);
  OGL(PixelStorei, GL_UNPACK_ALIGNMENT, 1);
  OGL(TexImage2D, GL_TEXTURE_2D, 0, GL_RGBA, ctx.lmdim.x, ctx.lmdim.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, ctx.lm);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  OGL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  //console::out("lm %i %i", ctx.lmdim.x, ctx.lmdim.y);
  //console::out("lmindex %i %i", lmindexdim.x, lmindexdim.y);
  b.rlmdim = rcp(vec2f(ctx.lmdim));
  delete [] ctx.lm;
}

/*--------------------------------------------------------------------------
 - world mesh handling (very simple for now)
 -------------------------------------------------------------------------*/
struct brickmeshctx {
  INLINE brickmeshctx(const world::lvl1grid &b) : b(b) {}
  INLINE void clear(s32 orientation) {
    face = orientation;
    MEMSET(indices, 0xff);
  }
  INLINE u16 get(vec3i p) const { return indices[p.x][p.y][p.z]; }
  INLINE void set(vec3i p, u16 idx) { indices[p.x][p.y][p.z] = idx; }
  const world::lvl1grid &b;
  vector<arrayf<10>> vbo;
  vector<u16> ibo;
  vector<u16> tex;
  u16 indices[world::lvl1+1][world::lvl1+1][world::lvl1+1];
  s32 face;
};

VAR(ldirx,-100,0,100);
VAR(ldiry,-100,50,100);
VAR(ldirz,-100,100,100);

static void buildfacemesh(brickmeshctx &ctx, vec3i xyz, vec3i idx) {
  if (!visibleface(xyz, ctx.face)) return;

  // build both triangles. we reuse already output vertices
  const int chan = ctx.face/2; // basically: x (0), y (1) or z (2)
  const int idx0 = 2*ctx.face+0, idx1 = 2*ctx.face+1;
  const vec3i tris[] = {cubetris[idx0], cubetris[idx1]};
  const auto tex = world::getcube(xyz).tex[ctx.face];
  loopi(2) { // build both triangles
    vec3f v[3]={zero,zero,zero}; // delay vertex creation for degenerated tris
    vec2f t[3]={zero,zero,zero}; // idem for texture coordinates
    vec2f l[3]={zero,zero,zero}; // idem for light map coordinates
    vec3f c[3]={zero,zero,zero}; // idem for vertex colors
    vec3i locals[3]={zero,zero,zero};
    bool isnew[3]={false,false,false};
    loopj(3) { // build each vertex
      const vec3i global = xyz+cubeiverts[tris[i][j]];
      const vec3i local = idx+cubeiverts[tris[i][j]];
      locals[j] = idx+cubeiverts[tris[i][j]];
      u16 id = ctx.get(locals[j]);
      if (id == 0xffff) {
        const vec3f pos = world::getpos(global);
        const vec2f tex = chan==0?pos.yz():(chan==1?pos.xz():pos.xy());
        id = ctx.vbo.length();
        v[j] = pos.xzy();
        t[j] = tex;
        const vec2i uv1 = getlmuv(local, ctx.face);
        //console::out("uv1 [%i %i]", uv1.x, uv1.y);

        l[j] = vec2f(getlmuv(local, ctx.face)) / vec2f(lmindexdim);
        c[j] = computecolor(vec3f(global), ctx.face);
        isnew[j] = true;
      } else
        v[j] = vec3f(ctx.vbo[id][0],ctx.vbo[id][1],ctx.vbo[id][2]);
    }
    const float mindist = 1.f/512.f;
    if (distance(v[0], v[1]) < mindist || // degenerated triangle?
        distance(v[1], v[2]) < mindist ||
        distance(v[2], v[0]) < mindist)
      continue;
    loopj(3) {
      if (isnew[j]) {
        ctx.set(locals[j], ctx.vbo.length());
        ctx.vbo.add(arrayf<10>(v[j],t[j],c[j],l[j]));
      }
      ctx.ibo.add(ctx.get(locals[j]));
      ctx.tex.add(tex);
    }
  }
}

static void radixsortibo(brickmeshctx &ctx) {
  const s32 bitn=8, bucketn=1<<bitn, passn=2, mask=bucketn-1;
  const auto len = ctx.ibo.length();
  u16 histo[bucketn];
  vector<u16> copytex(len), copyibo(len);
  vector<u16> *pptex[] = {&ctx.tex, &copytex};
  vector<u16> *ppibo[] = {&ctx.ibo, &copyibo};
  loopi(passn) {
    auto &fromtex = *pptex[i], &totex = *pptex[(i+1)%2];
    auto &fromibo = *ppibo[i], &toibo = *ppibo[(i+1)%2];
    u32 const shr = i*bitn;
    MEMZERO(histo); // compute the histogram
    loopj(len) histo[(fromtex[j]>>shr)&mask]++;
    u32 pred = histo[0];
    histo[0] = 0;
    loopj(bucketn-1) {
      const u32 next = histo[j+1];
      histo[j+1] = histo[j] + pred;
      pred = next;
    }
    loopj(len) { // sort using the histogram
      const u32 k = histo[(fromtex[j]>>shr)&mask]++;
      totex[k] = fromtex[j];
      toibo[k] = fromibo[j];
    }
  }
}

static void buildgridmesh(world::lvl1grid &b, vec3i org) {
  brickmeshctx ctx(b);
  loopi(6) {
    ctx.clear(i);
    loopxyz(0, b.size(), buildfacemesh(ctx, org+xyz, xyz));
  }
  if (ctx.vbo.length() == 0 || ctx.ibo.length() == 0) {
    if (b.vbo) OGL(DeleteBuffers, 1, &b.vbo);
    if (b.ibo) OGL(DeleteBuffers, 1, &b.ibo);
    b.vbo = b.ibo = 0;
    return;
  }
  if (ctx.vbo.length() > 0xffff) fatal("too many vertices in the VBO");
  radixsortibo(ctx);
  if (b.vbo) OGL(DeleteBuffers, 1, &b.vbo);
  if (b.ibo) OGL(DeleteBuffers, 1, &b.ibo);
  OGL(GenBuffers, 1, &b.vbo);
  OGL(GenBuffers, 1, &b.ibo);
  ogl::bindbuffer(ogl::ARRAY_BUFFER, b.vbo);
  ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, b.ibo);
  OGL(BufferData, GL_ARRAY_BUFFER, ctx.vbo.length()*sizeof(arrayf<10>), &ctx.vbo[0][0], GL_STATIC_DRAW);
  OGL(BufferData, GL_ELEMENT_ARRAY_BUFFER, ctx.ibo.length()*sizeof(u16), &ctx.ibo[0], GL_STATIC_DRAW);
  bindbuffer(ogl::ARRAY_BUFFER, 0);
  bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, 0);
  s32 n=1, tex=ctx.tex[0], len=ctx.ibo.length()-1;
  b.draws.setsize(0);
  loopi(len)
    if (ctx.tex[i+1]!=tex) {
      b.draws.add(vec2i(n,tex));
      tex=ctx.tex[i+1];
      n=1;
    } else
      ++n;
  b.draws.add(vec2i(n,tex));
}

static void buildbrick(world::lvl1grid &b, vec3i org) {
  if (b.dirty==0 && !forcebuild) return;
  console::out("START");
  buildlightmap(b, org);
  buildgridmesh(b, org);
  console::out("END");
  b.dirty = 0;
}

static void buildgrid(void) {
  ldir = normalize(vec3f(float(ldirx), float(ldiry), float(ldirz)));
  forallbricks(buildbrick);
  forcebuild = 0;
}
COMMAND(buildgrid, ARG_NONE);

static const char gridvert[] = {
  "uniform mat4 u_mvp;\n"
  VS_IN " vec3 vs_pos;\n"
  VS_IN " vec4 vs_col;\n"
  VS_IN " vec2 vs_tex;\n"
  VS_IN " vec2 vs_lm;\n"
  VS_OUT " vec4 fs_col;\n"
  VS_OUT " vec2 fs_tex;\n"
  VS_OUT " vec2 fs_lm;\n"
  "void main() {\n"
  "  fs_tex = vs_tex;\n"
  "  fs_col = vs_col;\n"
  "  fs_lm = vs_lm;\n"
  "  gl_Position = u_mvp*vec4(vs_pos,1.0);\n"
  "}\n"
};
static const char gridfrag[] = {
  "uniform sampler2D u_diffuse;\n"
  "uniform sampler2D u_lmindex;\n"
  "uniform sampler2D u_lm;\n"
  "uniform vec2 u_rlmdim;\n"
  PS_IN " vec2 fs_tex;\n"
  PS_IN " vec4 fs_col;\n"
  PS_IN " vec2 fs_lm;\n"
  IF_NOT_EMSCRIPTEN("out vec4 rt_c;\n")
  "void main() {\n"
  " vec4 lmindex = texture2D(u_lmindex, fs_lm);\n"
  //IF_NOT_EMSCRIPTEN(" vec2 lmuv = 65025.0*lmindex.yw+lmindex.xz*255.0;\n")
  //IF_NOT_EMSCRIPTEN(" vec4 lm = (lmuv*u_rlmdim).xyxy;\n")//texture(u_lm, lmuv*u_rlmdim);\n")
  // IF_NOT_EMSCRIPTEN(" vec4 lm = texture(u_lm, lmuv*u_rlmdim);\n")
  IF_NOT_EMSCRIPTEN(" vec4 lm = texture2D(u_lm, lmindex.xy+fract(fs_lm*256.0)/256.0);\n")
  IF_NOT_EMSCRIPTEN(" vec4 col = lm*fs_col;\n") //texture(u_diffuse, fs_tex);\n")
  IF_EMSCRIPTEN("  vec4 col = fs_col*texture2D(u_diffuse, fs_tex);\n")
  IF_NOT_EMSCRIPTEN("  rt_c = col;\n")
  IF_EMSCRIPTEN("  gl_FragColor = col;\n")
  "}\n"
};
static struct gridshader : shader {
  GLuint u_lmindex, u_lm, u_rlmdim;
} gridshader;

static void drawgrid(void) {
  using namespace world;
  bindshader(gridshader);
  setattribarray()(POS0, TEX0, TEX1, COL);
  forallbricks([&](const lvl1grid &b, const vec3i org) {
    bindbuffer(ogl::ARRAY_BUFFER, b.vbo);
    bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, b.ibo);
    bindtexture(GL_TEXTURE_2D, 1, b.lmindex);
    bindtexture(GL_TEXTURE_2D, 2, b.lm);
    OGL(Uniform2fv, gridshader.u_rlmdim, 1, &b.rlmdim.x);
    OGL(VertexAttribPointer, COL, 3, GL_FLOAT, 0, sizeof(float[10]), (const void*) sizeof(float[5]));
    OGL(VertexAttribPointer, TEX0, 2, GL_FLOAT, 0, sizeof(float[10]), (const void*) sizeof(float[3]));
    OGL(VertexAttribPointer, TEX1, 2, GL_FLOAT, 0, sizeof(float[10]), (const void*) sizeof(float[8]));
    OGL(VertexAttribPointer, POS0, 3, GL_FLOAT, 0, sizeof(float[10]), (const void*) 0);
    u32 offset = 0;
    loopi(b.draws.length()) {
      const auto fake = (const void*)(uintptr_t(offset*sizeof(u16)));
      const u32 n = b.draws[i].x;
      const u32 tex = b.draws[i].y;
      bindgametexture(GL_TEXTURE_2D, ogl::lookuptex(tex));
      drawelements(GL_TRIANGLES, n, GL_UNSIGNED_SHORT, fake);
      xtraverts += n;
      offset += n;
    }
  });
}

/*--------------------------------------------------------------------------
 - render md2 model
 -------------------------------------------------------------------------*/
void rendermd2(const float *pos0, const float *pos1, float lerp, int n) {
  OGL(VertexAttribPointer, TEX0, 2, GL_FLOAT, 0, sizeof(float[5]), pos0);
  OGL(VertexAttribPointer, POS0, 3, GL_FLOAT, 0, sizeof(float[5]), pos0+2);
  OGL(VertexAttribPointer, POS1, 3, GL_FLOAT, 0, sizeof(float[5]), pos1+2);
  setattribarray()(POS0, POS1, TEX0);
  bindshader(FOG|KEYFRAME|COL|DIFFUSETEX);
  OGL(Uniform1f, bindedshader->u_delta, lerp);
  drawarrays(GL_TRIANGLES, 0, n);
}

// flush all the states required for the draw call
static void flush(void) {
  if (dirty.any == 0) return; // fast path
  if (dirty.flags.shader) {
    OGL(UseProgram, bindedshader->program);
    dirty.flags.shader = 0;
  }
  if ((bindedshader->rules & FOG) && (dirty.flags.fog || dirty.flags.mvp)) {
    const mat4x4f &mv = vp[MODELVIEW];
    const vec4f zaxis(mv.vx.z,mv.vy.z,mv.vz.z,mv.vw.z);
    OGL(Uniform2fv, bindedshader->u_fogstartend, 1, &fogstartend.x);
    OGL(Uniform4fv, bindedshader->u_fogcolor, 1, &fogcolor.x);
    OGL(Uniform4fv, bindedshader->u_zaxis, 1, &zaxis.x);
    dirty.flags.fog = 0;
  }
  if (dirty.flags.mvp) {
    viewproj = vp[PROJECTION]*vp[MODELVIEW];
    OGL(UniformMatrix4fv, bindedshader->u_mvp, 1, GL_FALSE, &viewproj.vx.x);
    dirty.flags.mvp = 0;
  }
  if (dirty.flags.overbright) {
    OGL(Uniform1f, bindedshader->u_overbright, overbrightf);
    dirty.flags.overbright = 0;
  }
}

void drawarrays(int mode, int first, int count) { flush();
  OGL(DrawArrays, mode, first, count);
}
void drawelements(int mode, int count, int type, const void *indices) {
  flush();
  OGL(DrawElements, mode, count, type, indices);
}

#if !defined (EMSCRIPTEN)
#define GL_PROC(FIELD,NAME,PROTOTYPE) PROTOTYPE FIELD = NULL;
#include "GL/ogl100.hxx"
#include "GL/ogl110.hxx"
#include "GL/ogl120.hxx"
#include "GL/ogl130.hxx"
#include "GL/ogl150.hxx"
#include "GL/ogl200.hxx"
#include "GL/ogl300.hxx"
#undef GL_PROC
#endif // EMSCRIPTEN

void init(int w, int h) {
#if !defined(EMSCRIPTEN)
// on Windows, we directly load from OpenGL 1.1 functions
#if defined(__WIN32__)
  #define GL_PROC(FIELD,NAME,PROTOTYPE) FIELD = (PROTOTYPE) NAME;
#else
  #define GL_PROC(FIELD,NAME,PROTOTYPE) \
    FIELD = (PROTOTYPE) SDL_GL_GetProcAddress(#NAME); \
    if (FIELD == NULL) fatal("OpenGL 2 is required");
#endif // __WIN32__

#include "GL/ogl100.hxx"
#include "GL/ogl110.hxx"

// Now, we load everything with the window manager on Windows too
#if defined(__WIN32__)
  #undef GL_PROC
  #define GL_PROC(FIELD,NAME,PROTOTYPE) \
    FIELD = (PROTOTYPE) SDL_GL_GetProcAddress(#NAME); \
    if (FIELD == NULL) fatal("OpenGL 2 is required");
#endif // __WIN32__

#include "GL/ogl100.hxx"
#include "GL/ogl110.hxx"
#include "GL/ogl120.hxx"
#include "GL/ogl130.hxx"
#include "GL/ogl150.hxx"
#include "GL/ogl200.hxx"
#include "GL/ogl300.hxx"

#undef GL_PROC
#endif

  OGL(Viewport, 0, 0, w, h);
  loopi(int(ARRAY_ELEM_N(generatedids))) generatedids[i] = 0;
  loopi(int(TEX_NUM)) bindedtexture[i] = 0;

#if defined (EMSCRIPTEN)
  OGL(ClearDepthf,1.f);
#else
  OGL(ClearDepth,1.f);
#endif
  enablev(GL_DEPTH_TEST, GL_CULL_FACE);
  OGL(DepthFunc, GL_LESS);
  OGL(CullFace, GL_FRONT);
  OGL(GetIntegerv, GL_MAX_TEXTURE_SIZE, &glmaxtexsize);
  dirty.any = ~0x0;
  purgetextures();
  buildsphere(1, 12, 6);
  loopi(shadern) buildubershader(shaders[i], i); // build uber-shaders

  // build water shader
  buildshader(watershader, watervert, uberfrag, DIFFUSETEX); // build water shader
  OGLR(watershader.u_delta, GetUniformLocation, watershader.program, "u_delta");
  OGLR(watershader.u_duv, GetUniformLocation, watershader.program, "u_duv");
  OGLR(watershader.u_dxy, GetUniformLocation, watershader.program, "u_dxy");
  OGLR(watershader.u_hf, GetUniformLocation, watershader.program, "u_hf");

  // build world shader
  compileshader(gridshader, gridvert, gridfrag, DIFFUSETEX|COLOR);
  OGL(BindAttribLocation, gridshader.program, TEX1, "vs_lm");
  linkshader(gridshader);
  setshaderuniform(gridshader);
  OGL(UseProgram, gridshader.program);
  OGLR(gridshader.u_lmindex, GetUniformLocation, gridshader.program, "u_lmindex");
  OGLR(gridshader.u_lm, GetUniformLocation, gridshader.program, "u_lm");
  OGLR(gridshader.u_rlmdim, GetUniformLocation, gridshader.program, "u_rlmdim");
  OGL(Uniform1i, gridshader.u_lmindex, 1);
  OGL(Uniform1i, gridshader.u_lm, 2);
  OGL(UseProgram, 0);
  imminit();
  loopi(ATTRIB_NUM) enabledattribarray[i] = 0;
  loopi(BUFFER_NUM) bindedvbo[i] = 0;
}

void clean(void) { OGL(DeleteBuffers, 1, &spherevbo); }

void drawsphere(void) {
  ogl::bindbuffer(ARRAY_BUFFER, spherevbo);
  ogl::bindshader(DIFFUSETEX);
  draw(GL_TRIANGLE_STRIP, 3, 2, spherevertn, NULL);
}

static void setupworld(void) {
  setattribarray()(POS0, COL, TEX0);
}

static void transplayer(void) {
  identity();
  rotate(game::player1->roll, zaxis);
  rotate(-game::player1->pitch, xaxis);
  rotate(game::player1->yaw, yaxis);
  translate(vec3f(-game::player1->o.x,
            (game::player1->state==CS_DEAD ? game::player1->eyeheight-0.2f : 0)-game::player1->o.z,
            -game::player1->o.y));
}

VARP(fov, 10, 105, 120);
VAR(fog, 64, 180, 1024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);
VARP(hudgun,0,1,1);

static const char *hudgunnames[] = {
  "hudguns/fist",
  "hudguns/shotg",
  "hudguns/chaing",
  "hudguns/rocket",
  "hudguns/rifle"
};

static void drawhudmodel(int start, int end, float speed, int base) {
  rr::rendermodel(hudgunnames[game::player1->gunselect], start, end, 0, 1.0f,
    game::player1->o.xzy(), game::player1->yaw+90.f, game::player1->pitch,
    false, 1.0f, speed, 0, base);
}

static void drawhudgun(float fovy, float aspect, int farplane) {
  if (!hudgun) return;

  enablev(GL_CULL_FACE);
  matrixmode(PROJECTION);
  identity();
  perspective(fovy, aspect, 0.3f, farplane);
  matrixmode(MODELVIEW);

  const int rtime = game::reloadtime(game::player1->gunselect);
  if (game::player1->lastaction &&
      game::player1->lastattackgun==game::player1->gunselect &&
      game::lastmillis()-game::player1->lastaction<rtime)
    drawhudmodel(7, 18, rtime/18.0f, game::player1->lastaction);
  else
    drawhudmodel(6, 1, 100, 0);

  matrixmode(PROJECTION);
  identity();
  perspective(fovy, aspect, 0.15f, farplane);
  matrixmode(MODELVIEW);
  disablev(GL_CULL_FACE);
}

void draw(int mode, int pos, int tex, size_t n, const float *data) {
  loopi(ATTRIB_NUM) disableattribarray(i);
  if (tex) {
    enableattribarray(TEX0);
    OGL(VertexAttribPointer, TEX0, tex, GL_FLOAT, 0, (pos+tex)*sizeof(float), data);
  }
  enableattribarray(POS0);
  OGL(VertexAttribPointer, POS0, pos, GL_FLOAT, 0, (pos+tex)*sizeof(float), data+tex);
  ogl::drawarrays(mode, 0, n);
}

VAR(renderparticles,0,1,1);
VAR(rendersky,0,1,1);
VAR(renderworld,0,1,1);
VAR(renderwater,0,1,1);

// enforce the gl states
static void forceglstate(void) {
  bindgametexture(GL_TEXTURE_2D,0);
  loopi(BUFFER_NUM) bindbuffer(i,0);
  loopi(ATTRIB_NUM) disableattribarray(i);
}

static void dofog(bool underwater) {
  fogstartend.x = float((fog+64)/8);
  fogstartend.y = 1.f/(float(fog)-fogstartend[0]);
  fogcolor.x = float(fogcolour>>16)/256.0f;
  fogcolor.y = float((fogcolour>>8)&255)/256.0f;
  fogcolor.z = float(fogcolour&255)/256.0f;
  fogcolor.w = 1.f;
  OGL(ClearColor, fogcolor.x, fogcolor.y, fogcolor.z, fogcolor.w);
  if (underwater) {
    fogstartend.x = 0.f;
    fogstartend.y = 1.f/float((fog+96)/8);
  }
}

VAR(linefill,0,0,1);

void drawframe(int w, int h, float curfps) {
  const float hf = world::waterlevel()-0.3f;
  const bool underwater = game::player1->o.z<hf;
  float fovy = (float)fov*h/w;
  float aspect = w/(float)h;

  buildgrid();
  forceglstate();
  dofog(underwater);
  OGL(Clear, (game::player1->outsidemap ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);

  if (underwater) {
    fovy += (float)sin(game::lastmillis()/1000.0)*2.0f;
    aspect += (float)sin(game::lastmillis()/1000.0+PI)*0.1f;
  }
  const int farplane = fog*5/2;
  matrixmode(PROJECTION);
  identity();
  perspective(fovy, aspect, 0.15f, farplane);
  matrixmode(MODELVIEW);
  transplayer();

  // render sky
  if (rendersky) {
    identity();
    rotate(-game::player1->pitch, xaxis);
    rotate(game::player1->yaw, yaxis);
    rotate(90.f, vec3f(1.f,0.f,0.f));
    OGL(VertexAttrib3f,COL,1.0f,1.0f,1.0f);
    rr::draw_envbox(14, fog*4/3);
  }
  transplayer();
  overbright(2.f);
  setupworld();
  bindshader(DIFFUSETEX|FOG);

  ogl::xtraverts = 0;
  game::renderclients();
  game::monsterrender();
  game::renderentities();
  rr::renderspheres(game::curtime());
  rr::renderents();
  enablev(GL_CULL_FACE);
  IF_NOT_EMSCRIPTEN(OGL(PolygonMode, GL_FRONT_AND_BACK, linefill?GL_LINE:GL_FILL));
  drawgrid();
  disablev(GL_CULL_FACE);

  drawhudgun(fovy, aspect, farplane);
  if (world::raycast) {
    world::castrayprout(fovy, aspect, farplane);
    world::raycast = 0;
  }

  int nquads = 0;

  if (renderparticles) {
    overbright(2.f);
    bindshader(DIFFUSETEX);
    rr::render_particles(game::curtime());
  }

  overbright(1.f);
  rr::drawhud(w, h, int(curfps), nquads, rr::curvert, underwater);
  enablev(GL_CULL_FACE);
}

} // namespace ogl
} // namespace cube

