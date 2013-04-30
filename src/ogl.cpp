#include "cube.hpp"
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

namespace cube {
namespace rr { extern int curvert; }
namespace ogl {

int xtraverts = 0;

/*-------------------------------------------------------------------------
 - very simple state tracking
 -------------------------------------------------------------------------*/
union {
  struct {
    uint shader:1; // will force to reload everything
    uint mvp:1;
    uint fog:1;
    uint overbright:1;
  } flags;
  uint any;
} dirty;
static uint bindedvbo[BUFFER_NUM];
static uint bindedtexture = 0;
static uint enabledattribarray[ATTRIB_NUM];
static struct shader *bindedshader = NULL;

void enableattribarray(uint target) {
  if (!enabledattribarray[target]) {
    enabledattribarray[target] = 1;
    OGL(EnableVertexAttribArray, target);
  }
}

void disableattribarray(uint target) {
  if (enabledattribarray[target]) {
    enabledattribarray[target] = 0;
    OGL(DisableVertexAttribArray, target);
  }
}

/*--------------------------------------------------------------------------
 - immediate mode and buffer support
 -------------------------------------------------------------------------*/
static const uint glbufferbinding[BUFFER_NUM] = {
  GL_ARRAY_BUFFER,
  GL_ELEMENT_ARRAY_BUFFER
};
void bindbuffer(uint target, uint buffer) {
  if (bindedvbo[target] != buffer) {
    OGL(BindBuffer, glbufferbinding[target], buffer);
    bindedvbo[target] = buffer;
  }
}

// we use two big circular buffers to handle immediate mode
static int bigvbooffset=0, bigibooffset=0;
static int drawibooffset=0, drawvbooffset=0;
static GLuint bigvbo=0u, bigibo=0u;

static void initbuffer(GLuint &bo, int target, int size) {
  if (bo == 0u) OGL(GenBuffers, 1, &bo);
  bindbuffer(target, bo);
  OGL(BufferData, glbufferbinding[target], size, NULL, GL_DYNAMIC_DRAW);
  bindbuffer(target, 0);
}

static void immbufferinit(int size) {
  initbuffer(bigvbo, ARRAY_BUFFER, size);
  initbuffer(bigibo, ELEMENT_ARRAY_BUFFER, size);
}

VARF(immbuffersize, 1*MB, 4*MB, 16*MB, immbufferinit(immbuffersize));

void immattrib(int attrib, int n, int type, int sz, int offset) {
  const void *fake = (const void *) intptr_t(drawvbooffset+offset);
  OGL(VertexAttribPointer, attrib, n, type, 0, sz, fake);
}

static void immsetdata(int target, int sz, const void *data) {
  assert(sz < immbuffersize);
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
}

void immvertices(int sz, const void *vertices) {
  immsetdata(ARRAY_BUFFER, sz, vertices);
}

void immdrawarrays(int mode, int count, int type) {
  drawarrays(mode,count,type);
}

void immdrawelements(int mode, int count, int type, const void *indices) {
  int sz = count;
  switch (type) {
    case GL_UNSIGNED_SHORT: sz*=sizeof(ushort); break;
    case GL_UNSIGNED_BYTE: sz*=sizeof(uchar); break;
    case GL_UNSIGNED_INT: sz*=sizeof(uint); break;
  };
  immsetdata(ELEMENT_ARRAY_BUFFER, sz, indices);
  const void *fake = (const void *) intptr_t(drawibooffset);
  drawelements(mode, count, type, fake);
}

void immdraw(int mode, int pos, int tex, int col, size_t n, const float *data) {
  const int sz = (pos+tex+col)*sizeof(float);
  immvertices(n*sz, data);
  if (pos) {
    immattrib(ogl::POS0, pos, GL_FLOAT, sz, (tex+col)*sizeof(float));
    enableattribarrayv(POS0);
  }
  if (tex) {
    immattrib(ogl::TEX, tex, GL_FLOAT, sz, col*sizeof(float));
    enableattribarrayv(TEX);
  }
  if (col) {
    immattrib(ogl::COL, col, GL_FLOAT, sz, 0);
    enableattribarrayv(COL);
  }
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

#if defined(EMSCRIPTEN)
static const uint ID_NUM = 2*MAXTEX;
static GLuint generated_ids[ID_NUM];
#endif // EMSCRIPTEN

void bindtexture(uint target, uint id) {
  if (bindedtexture == id) return;
  bindedtexture = id;
#if defined(EMSCRIPTEN)
  if (id >= ID_NUM) fatal("out of bound texture ID");
  OGL(BindTexture, target, generated_ids[id]);
#else
  OGL(BindTexture, target, id);
#endif
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
#else
  if (tnum >= int(ID_NUM)) fatal("out of bound texture ID");
  if (generated_ids[tnum] == 0u)
    OGL(GenTextures, 1, generated_ids + tnum);
#endif // EMSCRIPTEN
  bindedtexture = 0;
  console::out("loading %s (%ix%i)", texname, s->w, s->h);
  ogl::bindtexture(GL_TEXTURE_2D, tnum);
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
  vector<vvecf<5>> v;
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
        v.add(vvecf<5>(s, t, x, y, z));
      }
      loopk(end) { // idem
        const float s = 1.f-float(i)/slices, t = 1.f-float(j+1)/stacks;
        const float x = sin2*sin0, y = sin2*cos0, z = zHigh;
        v.add(vvecf<5>(s, t, x, y, z));
      }
      spherevertn += start+end;
    }
  }

  const size_t sz = sizeof(vvecf<5>) * v.length();
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
  "uniform mat4 MVP;\n"
  "#if USE_FOG\n"
  "  uniform vec4 zaxis;\n"
  "  " VS_OUT " float fogz;\n"
  "#endif\n"
  "#if USE_KEYFRAME\n"
  "  uniform float delta;\n"
  "  " VS_IN " vec3 p0, p1;\n"
  "#else\n"
  "  " VS_IN " vec3 p;\n"
  "#endif\n"
  VS_IN " vec4 incol;\n"
  "#if USE_DIFFUSETEX\n"
  "  " VS_OUT " vec2 texcoord;\n"
  "  " VS_IN " vec2 t;\n"
  "#endif\n"
  VS_OUT " vec4 outcol;\n"
  "void main() {\n"
  "#if USE_DIFFUSETEX\n"
  "  texcoord = t;\n"
  "#endif\n"
  "  outcol = incol;\n"
  "#if USE_KEYFRAME\n"
  "  vec3 p = mix(p0,p1,delta);\n"
  "#endif\n"
  "#if USE_FOG\n"
  "  fogz = dot(zaxis,vec4(p,1.0));\n"
  "#endif\n"
  "  gl_Position = MVP * vec4(p,1.0);\n"
  "}\n"
};
static const char uberfrag[] = {
  "#if USE_DIFFUSETEX\n"
  "  uniform sampler2D diffuse;\n"
  "  " PS_IN " vec2 texcoord;\n"
  "#endif\n"
  "#if USE_FOG\n"
  "  uniform vec4 fogcolor;\n"
  "  uniform vec2 fogstartend;\n"
  "  " PS_IN " float fogz;\n"
  "#endif\n"
  "uniform float overbright;\n"
  PS_IN " vec4 outcol;\n"
  IF_NOT_EMSCRIPTEN("out vec4 c;\n")
  "void main() {\n"
  "  vec4 col;\n"
  "#if USE_DIFFUSETEX\n"
  IF_NOT_EMSCRIPTEN("  col = texture(diffuse, texcoord);\n")
  IF_EMSCRIPTEN("  col = texture2D(diffuse, texcoord);\n")
  "  col *= outcol;\n"
  "#else\n"
  "  col = outcol;\n"
  "#endif\n"
  "#if USE_FOG\n"
  "  float factor = clamp((-fogz-fogstartend.x)*fogstartend.y,0.0,1.0)\n;"
  "  col.xyz = mix(col.xyz,fogcolor.xyz,factor);\n"
  "#endif\n"
  "  col.xyz *= overbright;\n"
  IF_NOT_EMSCRIPTEN("  c = col;\n")
  IF_EMSCRIPTEN("  gl_FragColor = col;\n")
  "}\n"
};

static struct shader {
  uint rules; // fog,keyframe...?
  GLuint program; // ogl program
  GLuint udiffuse, udelta, umvp, uoverbright; // uniforms
  GLuint uzaxis, ufogstartend, ufogcolor; // uniforms
} shaders[shadern];

static const char watervert[] = { // use DIFFUSETEX
  "#define PI 3.14159265\n"
  "uniform mat4 MVP;\n"
  "uniform vec2 duv, dxy;\n"
  "uniform float hf, delta;\n"
  VS_IN " vec2 p, t;\n"
  VS_IN " vec4 incol;\n"
  VS_OUT " vec2 texcoord;\n"
  VS_OUT " vec4 outcol;\n"
  "float dx(float x) { return x+sin(x*2.0+delta/1000.0)*0.04; }\n"
  "float dy(float x) { return x+sin(x*2.0+delta/900.0+PI/5.0)*0.05; }\n"
  "void main() {\n"
  "  texcoord = vec2(dx(t.x+duv.x),dy(t.y+duv.y));\n"
  "  vec2 absp = dxy+p;\n"
  "  vec3 pos = vec3(absp.x,hf-sin(absp.x*absp.y*0.1+delta/300.0)*0.2,absp.y);\n"
  "  outcol = incol;\n"
  "  gl_Position = MVP * vec4(pos,1.0);\n"
  "}\n"
};
static struct watershader : shader {
  GLuint uduv, udxy, uhf;
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

static void buildshaderattrib(shader &shader) {
  if (shader.rules&KEYFRAME) {
    OGL(BindAttribLocation, shader.program, POS0, "p0");
    OGL(BindAttribLocation, shader.program, POS1, "p1");
  } else
    OGL(BindAttribLocation, shader.program, POS0, "p");
  if (shader.rules&DIFFUSETEX)
    OGL(BindAttribLocation, shader.program, TEX, "t");
  OGL(BindAttribLocation, shader.program, COL, "incol");
#if !defined(EMSCRIPTEN)
  OGL(BindFragDataLocation, shader.program, 0, "c");
#endif // EMSCRIPTEN
  OGL(LinkProgram, shader.program);
  OGL(ValidateProgram, shader.program);
  OGL(UseProgram, shader.program);
  OGLR(shader.uoverbright, GetUniformLocation, shader.program, "overbright");
  OGLR(shader.umvp, GetUniformLocation, shader.program, "MVP");
  if (shader.rules&KEYFRAME)
    OGLR(shader.udelta, GetUniformLocation, shader.program, "delta");
  else
    shader.udelta = 0;
  if (shader.rules&DIFFUSETEX) {
    OGLR(shader.udiffuse, GetUniformLocation, shader.program, "diffuse");
    OGL(Uniform1i, shader.udiffuse, 0);
  }
  if (shader.rules&FOG) {
    OGLR(shader.uzaxis, GetUniformLocation, shader.program, "zaxis");
    OGLR(shader.ufogcolor, GetUniformLocation, shader.program, "fogcolor");
    OGLR(shader.ufogstartend, GetUniformLocation, shader.program, "fogstartend");
  }
  OGL(UseProgram, 0);
}

static void buildshader(shader &shader, const char *vertsrc, const char *fragsrc, uint rules) {
  memset(&shader, 0, sizeof(struct shader));
  shader.program = loadprogram(vertsrc, fragsrc, rules);
  shader.rules = rules;
  buildshaderattrib(shader);
}

static void buildubershader(shader &shader, uint rules) {
  buildshader(shader, ubervert, uberfrag, rules);
}

// display the binded md2 model
void rendermd2(const float *pos0, const float *pos1, float lerp, int n) {
  OGL(VertexAttribPointer, TEX, 2, GL_FLOAT, 0, sizeof(float[5]), pos0);
  OGL(VertexAttribPointer, POS0, 3, GL_FLOAT, 0, sizeof(float[5]), pos0+2);
  OGL(VertexAttribPointer, POS1, 3, GL_FLOAT, 0, sizeof(float[5]), pos1+2);
  enableattribarrayv(POS0, POS1, TEX);
  disableattribarrayv(COL);
  bindshader(FOG|KEYFRAME|COL|DIFFUSETEX);
  OGL(Uniform1f, bindedshader->udelta, lerp);
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
    OGL(Uniform2fv, bindedshader->ufogstartend, 1, &fogstartend.x);
    OGL(Uniform4fv, bindedshader->ufogcolor, 1, &fogcolor.x);
    OGL(Uniform4fv, bindedshader->uzaxis, 1, &zaxis.x);
    dirty.flags.fog = 0;
  }
  if (dirty.flags.mvp) {
    viewproj = vp[PROJECTION]*vp[MODELVIEW];
    OGL(UniformMatrix4fv, bindedshader->umvp, 1, GL_FALSE, &viewproj.vx.x);
    dirty.flags.mvp = 0;
  }
  if (dirty.flags.overbright) {
    OGL(Uniform1f, bindedshader->uoverbright, overbrightf);
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

template <int x> struct powerof2policy { enum {value = !!((x&(x-1))==0)}; };
#define GRIDPOLICY(x,y,z) \
  static_assert(powerof2policy<x>::value,"x must be power of 2");\
  static_assert(powerof2policy<y>::value,"y must be power of 2");\
  static_assert(powerof2policy<z>::value,"z must be power of 2");

// cube material
enum  {
  EMPTY = 0, // nothing at all
  FULL = 1 // contains a deformed cube
};

// data contained in the most lower grid
struct brickcube {
  INLINE brickcube(u8 m) : p(0,0,0), mat(m), extra(0) {}
  INLINE brickcube(vec3<u8> o = vec3<u8>(0,0,0), u8 m = EMPTY) : p(o), mat(m), extra(0) {}
  vec3<u8> p;
  u8 mat;
  u32 extra;
};
// empty (== air) cube and no displacement
static const brickcube emptycube;

#define loopxyz(org, end, body)\
  for (int X = vec3i(org).x; X < vec3i(end).x; ++X)\
  for (int Y = vec3i(org).y; Y < vec3i(end).y; ++Y)\
  for (int Z = vec3i(org).z; Z < vec3i(end).z; ++Z) do {\
    const vec3i xyz(X,Y,Z);\
    body;\
  } while (0)
#undef INLINE
#define INLINE

// actually contains the data (geometries)
template <int x, int y, int z>
struct brick : public noncopyable {
  INLINE brick(void) {
    vbo = ibo = 0u;
    elemnum = 0;
  }
  static INLINE vec3i size(void) { return vec3i(x,y,z); }
  static INLINE vec3i global(void) { return size(); }
  static INLINE vec3i local(void) { return size(); }
  static INLINE vec3i cuben(void) { return size(); }
  static INLINE vec3i subcuben(void) { return vec3i(one); }
  INLINE brickcube get(vec3i v) const { return elem[v.x][v.y][v.z]; }
  INLINE brickcube subgrid(vec3i v) const { return get(v); }
  INLINE void set(vec3i v, const brickcube &cube) { elem[v.x][v.y][v.z]=cube; }
  INLINE brick &getbrick(vec3i idx) { return *this; }
  template <typename F> INLINE void forallcubes(const F &f, vec3i org) {
    loopxyz(zero, size(), {
      auto cube = get(xyz);
      if (cube.mat != EMPTY)
        f(cube, org + xyz);
    });
  }
  template <typename F> INLINE void forallbricks(const F &f, vec3i org) {
    f(*this, org);
  }
  brickcube elem[x][y][z];
  GRIDPOLICY(x,y,z);
  enum {cubenx=x, cubeny=y, cubenz=z};
  GLuint vbo, ibo;
  u32 elemnum;
};

// recursive sparse grid
template <typename T, int locx, int locy, int locz, int globx, int globy, int globz>
struct grid : public noncopyable {
  typedef T childtype;
  enum {cubenx=locx*T::cubenx, cubeny=locy*T::cubeny, cubenz=locz*T::cubenz};
  INLINE grid(void) { memset(elem, 0, sizeof(elem)); }
  static INLINE vec3i local(void) { return vec3i(locx,locy,locz); }
  static INLINE vec3i global(void) { return vec3i(globx,globy,globz); }
  static INLINE vec3i cuben(void) { return vec3i(cubenx, cubeny, cubenz); }
  static INLINE vec3i subcuben(void) { return T::cuben(); }
  INLINE vec3i index(vec3i p) const {
    return any(p<vec3i(zero))?local():p*local()/global();
  }
  INLINE T *subgrid(vec3i idx) const {
    if (any(idx>=local())) return NULL;
    return elem[idx.x][idx.y][idx.z];
  }
  INLINE brickcube get(vec3i v) const {
    auto idx = index(v);
    auto e = subgrid(idx);
    if (e == NULL) return emptycube;
    return e->get(v-idx*subcuben());
  }
  INLINE void set(vec3i v, const brickcube &cube) {
    auto idx = index(v);
    if (any(idx>=local())) return;
    auto &e = elem[idx.x][idx.y][idx.z];
    if (e == NULL) e = new T;
    e->set(v-idx*subcuben(), cube);
  }
  template <typename F> INLINE void forallcubes(const F &f, vec3i org) {
    loopxyz(zero, local(), if (T *e = subgrid(xyz))
      e->forallcubes(f, org + xyz*global()/local()));
  }
  template <typename F> INLINE void forallbricks(const F &f, vec3i org) {
    loopxyz(zero, local(), if (T *e = subgrid(xyz))
      e->forallbricks(f, org + xyz*global()/local()););
  }
  T *elem[locx][locy][locz]; // each elem may be null when empty
  GRIDPOLICY(locx,locy,locz);
  GRIDPOLICY(globx,globy,globz);
};

// all levels of details for our world
static const int lvl1x = 16, lvl1y = 16, lvl1z = 16;
static const int lvl2x = 4,  lvl2y = 4,  lvl2z = 4;
static const int lvl3x = 4,  lvl3y = 4,  lvl3z = 4;
static const int lvlt1x = lvl1x, lvlt1y = lvl1y, lvlt1z = lvl1z;

// compute the total number of cubes for each level
#define LVL_TOT(N, M)\
static const int lvlt##N##x = lvl##N##x * lvlt##M##x;\
static const int lvlt##N##y = lvl##N##y * lvlt##M##y;\
static const int lvlt##N##z = lvl##N##z * lvlt##M##z;
LVL_TOT(2,1)
LVL_TOT(3,2)
#undef LVL_TOT

// define a type for one level of the hierarchy
#define GRID(CHILD,N)\
typedef grid<CHILD,lvl##N##x,lvl##N##y,lvl##N##z,lvlt##N##x,lvlt##N##y,lvlt##N##z> lvl##N##grid;
typedef brick<lvl1x,lvl1y,lvl1z> lvl1grid;
GRID(lvl1grid,2)
GRID(lvl2grid,3)
#undef GRID

// our world and its total dimension
static lvl3grid root;
static const int worldsizex=lvlt3x, worldsizey=lvlt3y, worldsizez=lvlt3z;
static const vec3i worldisize(worldsizex,worldsizey,worldsizez);
static const vec3f worldfsize(worldsizex,worldsizey,worldsizez);

template <typename F> static void forallbricks(const F &f) { root.forallbricks(f, zero); }
template <typename F> static void forallcubes(const F &f) { root.forallcubes(f, zero); }

static const int cubedirnum = 6;
static const int cubevertnum = 8;
static const int cubetrinum = 12;
static const vec3i cubeverts[cubevertnum] = {
  vec3i(0,0,0)/*0*/, vec3i(0,0,1)/*1*/, vec3i(0,1,1)/*2*/, vec3i(0,1,0)/*3*/,
  vec3i(1,0,0)/*4*/, vec3i(1,0,1)/*5*/, vec3i(1,1,1)/*6*/, vec3i(1,1,0)/*7*/
};
static const vec3i cubetris[cubetrinum] = {
  vec3i(0,1,2), vec3i(0,2,3), vec3i(4,7,6), vec3i(4,6,5), // -x,+x triangles
  vec3i(0,4,5), vec3i(0,5,1), vec3i(2,6,7), vec3i(2,7,3), // -y,+y triangles
  vec3i(3,7,4), vec3i(3,4,0), vec3i(1,5,6), vec3i(1,6,2)  // -z,+z triangles
};
static const vec3i cubenorms[cubedirnum] = {
  vec3i(-1,0,0), vec3i(+1,0,0),
  vec3i(0,-1,0), vec3i(0,+1,0),
  vec3i(0,0,-1), vec3i(0,0,+1)
};

struct brickmeshctx {
  brickmeshctx(const lvl1grid &b) : b(b) {}
  void clear(int orientation) {
    dir = orientation;
    memset(indices, 0xff, sizeof(indices));
  }
  INLINE u16 get(vec3i p) const { return indices[p.x][p.y][p.z]; }
  INLINE void set(vec3i p, u16 idx) { indices[p.x][p.y][p.z] = idx; }
  const lvl1grid &b;
  vector<vvecf<5>> vbo;
  vector<u16> ibo;
  u16 indices[lvl1x+1][lvl1y+1][lvl1z+1];
  int dir;
};

static INLINE void buildfaces(brickmeshctx &ctx, vec3i xyz, vec3i idx) {
  const int chan = ctx.dir/2; // basically: x (==0), y (==1) or z (==2)
  if (root.get(xyz).mat == EMPTY) return; // nothing here
  if (root.get(xyz+cubenorms[ctx.dir]).mat != EMPTY) return; // invisible face

  // build both triangles. we reuse already output vertices
  const int idx0 = 2*ctx.dir+0, idx1 = 2*ctx.dir+1;
  const vec3i tris[] = {cubetris[idx0], cubetris[idx1]};
  loopi(2) { // build both triangles
    loopj(3) { // build each vertex
      const vec3i local = idx+cubeverts[tris[i][j]];
      const vec3i global = xyz+cubeverts[tris[i][j]];
      u16 id = ctx.get(local);
      if (id == 0xffff) {
        const vec3f pos = vec3f(global);//+vec3f(root.get(global).p);
        const vec2f tex = chan==0?pos.yz():(chan==1?pos.xz():pos.xy());
        id = ctx.vbo.length();
        ctx.vbo.add(vvecf<5>(pos.xzy(),tex));
        ctx.set(local, id);
      }
      ctx.ibo.add(id);
    }
  }
}

static void buildgridmesh(lvl1grid &b, vec3i org) {
  brickmeshctx ctx(b);
  loopi(6) {
    ctx.clear(i);
    loopxyz(0, b.size(), buildfaces(ctx, org+xyz, xyz));
  }
  if (ctx.vbo.length() > 0xffff) fatal("too many vertices in the VBO");
  OGL(GenBuffers, 1, &b.vbo);
  OGL(GenBuffers, 1, &b.ibo);
  ogl::bindbuffer(ogl::ARRAY_BUFFER, b.vbo);
  ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, b.ibo);
  OGL(BufferData, GL_ARRAY_BUFFER, ctx.vbo.length()*sizeof(vvecf<5>), &ctx.vbo[0][0], GL_STATIC_DRAW);
  OGL(BufferData, GL_ELEMENT_ARRAY_BUFFER, ctx.ibo.length()*sizeof(u16), &ctx.ibo[0], GL_STATIC_DRAW);
  bindbuffer(ogl::ARRAY_BUFFER, 0);
  bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, 0);
  b.elemnum = ctx.ibo.length();
}

static void fillgrid(void) {
  loop(x,worldisize.x) loop(y,worldisize.y) root.set(vec3i(x,y,0), brickcube(FULL));
  for (int x = 8; x < worldisize.x; x += 32)
  for (int y = 8; y < worldisize.y; y += 32)
    loop(z,worldisize.z) root.set(vec3i(x,y,z), brickcube(FULL));
  forallbricks(buildgridmesh);
}

static void drawgrid(void) {
  static bool initialized = false;
  if (!initialized) {
    texturereset();
    texture("0", "ikbase/ik_floor_met128e.jpg");
    fillgrid();
    initialized = true;
  }
  ogl::bindtexture(GL_TEXTURE_2D, ogl::lookuptex(0));
  enableattribarrayv(POS0, TEX);
  disableattribarrayv(COL, POS1);
  bindshader(DIFFUSETEX);
  forallbricks([&](const lvl1grid &b, const vec3i org) {
    ogl::bindbuffer(ogl::ARRAY_BUFFER, b.vbo);
    ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, b.ibo);
    OGL(VertexAttribPointer, TEX, 2, GL_FLOAT, 0, sizeof(float[5]), (const void*) sizeof(float[3]));
    OGL(VertexAttribPointer, POS0, 3, GL_FLOAT, 0, sizeof(float[5]), (const void*) 0);
    ogl::drawelements(GL_TRIANGLES, b.elemnum, GL_UNSIGNED_SHORT, 0);
    ogl::xtraverts += b.elemnum;
  });
}

struct ray {
  INLINE ray(void) {}
  INLINE ray(vec3f org, vec3f dir, float near = 0.f, float far = FLT_MAX)
    : org(org), dir(dir), rdir(rcp(dir)), tnear(near), tfar(far) {}
  vec3f org, dir, rdir;
  float tnear, tfar;
};

struct camera {
  INLINE camera(vec3f org, vec3f up, vec3f view, float fov, float ratio) :
    org(org), up(up), view(view), fov(fov), ratio(ratio)
  {
    const float left = -ratio * 0.5f;
    const float top = 0.5f;
    dist = 0.5f / tan(fov * float(pi) / 360.f);
    view = normalize(view);
    up = normalize(up);
    xaxis = cross(view, up);
    xaxis = normalize(xaxis);
    zaxis = cross(view, xaxis);
    zaxis = normalize(zaxis);
    imgplaneorg = dist*view + left*xaxis - top*zaxis;
    xaxis *= ratio;
  }
  INLINE ray generate(int w, int h, int x, int y) const {
    const float rw = rcp(float(w)), rh = rcp(float(h));
    const vec3f sxaxis = xaxis*rw, szaxis = zaxis*rh;
    const vec3f dir = normalize(imgplaneorg + float(x)*sxaxis + float(y)*szaxis);
    return ray(org, dir);
  }
  vec3f org, up, view, imgplaneorg, xaxis, zaxis;
  float fov, ratio, dist;
};

struct aabb {
  INLINE aabb(vec3f m, vec3f M) : pmin(m), pmax(M) {}
  vec3f pmin, pmax;
};
struct isecres {
  INLINE isecres(bool isec = false, float t = 0.f) : t(t), isec(isec) {}
  float t;
  bool isec;
};
INLINE isecres slab(const aabb &box, vec3f org, vec3f rdir, float t) {
  const vec3f l1 = (box.pmin-org)*rdir;
  const vec3f l2 = (box.pmax-org)*rdir;
  const float tfar = reducemin(max(l1,l2));
  const float tnear = reducemax(min(l1,l2));
  return isecres((tfar >= tnear) & (tfar >= 0.f) & (tnear < t), max(0.f,tnear));
}

static void writebmp(const int *data, int width, int height, const char *filename) {
  int x, y;
  FILE *fp = fopen(filename, "wb");
  assert(fp);
  struct bmphdr {
    int filesize;
    short as0, as1;
    int bmpoffset;
    int headerbytes;
    int width;
    int height;
    short nplanes;
    short bpp;
    int compression;
    int sizeraw;
    int hres;
    int vres;
    int npalcolors;
    int nimportant;
  };

  const char magic[2] = { 'B', 'M' };
  char *raw = (char *) malloc(width * height * sizeof(int)); // at most
  assert(raw);
  char *p = raw;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int c = *data++;
      *p++ = ((c >> 16) & 0xff);
      *p++ = ((c >> 8) & 0xff);
      *p++ = ((c >> 0) & 0xff);
    }
    while (x & 3) {
      *p++ = 0;
      x++;
    } // pad to dword
  }
  int sizeraw = p - raw;
  int scanline = (width * 3 + 3) & ~3;
  assert(sizeraw == scanline * height);

  struct bmphdr hdr;

  hdr.filesize = scanline * height + sizeof(hdr) + 2;
  hdr.as0 = 0;
  hdr.as1 = 0;
  hdr.bmpoffset = sizeof(hdr) + 2;
  hdr.headerbytes = 40;
  hdr.width = width;
  hdr.height = height;
  hdr.nplanes = 1;
  hdr.bpp = 24;
  hdr.compression = 0;
  hdr.sizeraw = sizeraw;
  hdr.hres = 0;
  hdr.vres = 0;
  hdr.npalcolors = 0;
  hdr.nimportant = 0;
  fwrite(&magic[0], 1, 2, fp);
  fwrite(&hdr, 1, sizeof(hdr), fp);
  fwrite(raw, 1, hdr.sizeraw, fp);
  fclose(fp);
  free(raw);
}

VAR(raycast, 0, 0, 1);

template <typename T> struct gridpolicy {
  enum { updatetmin = 1 };
  static INLINE vec3f cellorg(vec3f boxorg, vec3i xyz, vec3f cellsize) {
    return boxorg+vec3f(xyz)*cellsize;
  }
};
template <> struct gridpolicy<lvl1grid> {
  enum { updatetmin = 1 };
  static INLINE vec3f cellorg(vec3f boxorg, vec3i xyz, vec3f cellsize) {
    return vec3f(zero);
  }
};

INLINE isecres intersect(const brickcube &cube, const vec3f &boxorg, const ray &ray, float t) {
  return isecres(cube.mat == FULL, t);
}

template <typename G>
NOINLINE isecres intersect(const G *grid, const vec3f &boxorg, const ray &ray, float t) {
  if (grid == NULL) return isecres(false);
  const bool update = gridpolicy<G>::updatetmin == 1;
  const vec3b signs = ray.dir > vec3f(zero);
  const vec3f cellsize = grid->subcuben();
  const vec3i step = select(signs, vec3i(one), -vec3i(one));
  const vec3i out = select(signs, grid->global(), -vec3i(one));
  const vec3f delta = abs(ray.rdir*cellsize);
  const vec3f entry = ray.org+t*ray.dir;
  vec3i xyz = min(vec3i((entry-boxorg)/cellsize), grid->local()-vec3i(one));
  const vec3f floorentry = vec3f(xyz)*cellsize+boxorg;
  const vec3f exit = floorentry + select(signs, cellsize, vec3f(zero));
  vec3f tmax = vec3f(t)+(exit-entry)*ray.rdir;
  tmax = select(ray.dir==vec3f(zero),vec3f(FLT_MAX),tmax);
  for (;;) {
    const vec3f cellorg = gridpolicy<G>::cellorg(boxorg, xyz, cellsize);
    const auto isec = intersect(grid->subgrid(xyz), cellorg, ray, t);
    if (isec.isec) return isec;
    if (tmax.x < tmax.y) {
      if (tmax.x < tmax.z) {
        xyz.x += step.x;
        if (xyz.x == out.x) return isecres(false);
        if (update) t = tmax.x;
        tmax.x += delta.x;
      } else {
        xyz.z += step.z;
        if (xyz.z == out.z) return isecres(false);
        if (update) t = tmax.z;
        tmax.z += delta.z;
      }
    } else {
      if (tmax.y < tmax.z) {
        xyz.y += step.y;
        if (xyz.y == out.y) return isecres(false);
        if (update) t = tmax.y;
        tmax.y += delta.y;
      } else {
        xyz.z += step.z;
        if (xyz.z == out.z) return isecres(false);
        if (update) t = tmax.z;
        tmax.z += delta.z;
      }
    }
  }
  return isecres(false);
}

static void castray(float fovy, float aspect, float farplane) {
  const int w = 1024, h = 768;
  int *pixels = (int*)malloc(w*h*sizeof(int));
  using namespace game;
  const mat3x3f r = mat3x3f::rotate(vec3f(0.f,0.f,1.f),game::player1->yaw)*
                    mat3x3f::rotate(vec3f(0.f,1.f,0.f),game::player1->roll)*
                    mat3x3f::rotate(vec3f(-1.f,0.f,0.f),game::player1->pitch);
  const camera cam(game::player1->o, -r.vz, -r.vy, fovy, aspect);
  const vec3f cellsize(one), boxorg(zero);
  const aabb box(boxorg, cellsize*vec3f(root.global()));
  const int start = SDL_GetTicks();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int offset = x+w*y;
      const ray ray = cam.generate(w, h, x, y);
      const isecres res = slab(box, ray.org, ray.rdir, ray.tfar);
      if (!res.isec) {
        pixels[offset] = 0;
        continue;
      }
      const auto isec = intersect(&root, box.pmin, ray, res.t);
      if (isec.isec) {
        const int d = min(int(isec.t), 255);
        pixels[offset] = d|(d<<8)|(d<<16)|(0xff<<24);
      } else
        pixels[offset] = 0;

    }
    //printf("\r%i%%               ",100*y/h);
  }
  const int ms = SDL_GetTicks()-start;
  printf("\n%i ms, %f ray/s\n", ms, 1000.f*(w*h)/ms);
  writebmp(pixels, w, h, "hop.bmp");
  free(pixels);
}

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
#if defined (EMSCRIPTEN)
  for (u32 i = 0; i < sizeof(generated_ids) / sizeof(generated_ids[0]); ++i)
    generated_ids[i] = 0;
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
  buildshader(watershader, watervert, uberfrag, DIFFUSETEX); // build water shader
  OGLR(watershader.udelta, GetUniformLocation, watershader.program, "delta");
  OGLR(watershader.uduv, GetUniformLocation, watershader.program, "duv");
  OGLR(watershader.udxy, GetUniformLocation, watershader.program, "dxy");
  OGLR(watershader.uhf, GetUniformLocation, watershader.program, "hf");
  immbufferinit(immbuffersize); // for immediate mode
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
  enableattribarrayv(POS0, COL, TEX);
  disableattribarrayv(POS1);
}

static const vec3f roll(0.f,0.f,1.f);
static const vec3f pitch(-1.f,0.f,0.f);
static const vec3f yaw(0.f,1.f,0.f);
static void transplayer(void) {
  identity();
  rotate(game::player1->roll,roll);
  rotate(game::player1->pitch,pitch);
  rotate(game::player1->yaw,yaw);
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
  if (tex) {
    enableattribarrayv(TEX);
    OGL(VertexAttribPointer, TEX, tex, GL_FLOAT, 0, (pos+tex)*sizeof(float), data);
  }
  enableattribarrayv(POS0);
  OGL(VertexAttribPointer, POS0, pos, GL_FLOAT, 0, (pos+tex)*sizeof(float), data+tex);
  disableattribarrayv(POS1, COL);
  ogl::drawarrays(mode, 0, n);
}

VAR(renderparticles,0,1,1);
VAR(rendersky,0,1,1);
VAR(renderworld,0,1,1);
VAR(renderwater,0,1,1);

// enforce the gl states
static void forceglstate(void) {
  bindtexture(GL_TEXTURE_2D,0);
  loopi(BUFFER_NUM) bindbuffer(i,0);
  loopi(ATTRIB_NUM) disableattribarrayv(i);
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

void drawframe(int w, int h, float curfps) {
  const float hf = world::waterlevel()-0.3f;
  const bool underwater = game::player1->o.z<hf;
  float fovy = (float)fov*h/w;
  float aspect = w/(float)h;

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
    rotate(game::player1->pitch, pitch);
    rotate(game::player1->yaw, yaw);
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
  ogl::drawgrid();
  disablev(GL_CULL_FACE);

  drawhudgun(fovy, aspect, farplane);
  if (raycast) {
    castray(fovy, aspect, farplane);
    raycast = 0;
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

