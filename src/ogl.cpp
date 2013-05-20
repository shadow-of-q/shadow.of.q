#include "cube.hpp"
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

namespace cube {

// XXX just for now
namespace world{
extern int raycast;
extern void castray(float fovy, float aspect, float farplane);
}
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
  disableattribarrayv(POS1,NOR);
  if (pos) {
    immattrib(ogl::POS0, pos, GL_FLOAT, (tex+col)*sizeof(float));
    enableattribarrayv(POS0);
  } else
    disableattribarray(POS0);
  if (tex) {
    immattrib(ogl::TEX, tex, GL_FLOAT, col*sizeof(float));
    enableattribarrayv(TEX);
  } else
    disableattribarray(TEX);
  if (col) {
    immattrib(ogl::COL, col, GL_FLOAT, 0);
    enableattribarrayv(COL);
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

#if defined(EMSCRIPTEN)
static const uint IDNUM = 2*MAXTEX;
static GLuint generatedids[IDNUM];
#endif // EMSCRIPTEN

void bindtexture(uint target, uint id) {
  if (bindedtexture == id) return;
  bindedtexture = id;
#if defined(EMSCRIPTEN)
  if (id >= IDNUM) fatal("out of bound texture ID");
  OGL(BindTexture, target, generatedids[id]);
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
  if (tnum >= int(IDNUM)) fatal("out of bound texture ID");
  if (generatedids[tnum] == 0u)
    OGL(GenTextures, 1, generatedids + tnum);
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


/*--------------------------------------------------------------------------
 - world mesh handling (very simple for now)
 -------------------------------------------------------------------------*/
struct brickmeshctx {
  brickmeshctx(const world::lvl1grid &b) : b(b) {}
  void clear(int orientation) {
    face = orientation;
    MEMSET(indices, 0xff);
  }
  INLINE u16 get(vec3i p) const { return indices[p.x][p.y][p.z]; }
  INLINE void set(vec3i p, u16 idx) { indices[p.x][p.y][p.z] = idx; }
  const world::lvl1grid &b;
  vector<arrayf<8>> vbo;
  vector<u16> ibo;
  vector<u16> tex;
  u16 indices[world::lvl1+1][world::lvl1+1][world::lvl1+1];
  s32 face;
};

static const vec3f ldir = normalize(vec3f(0.5f,0.2f,0.5f));

INLINE vec3f computecolor(vec3f pos, int face) {
#if 0
  const auto n = 0.01f*vec3f(cubenorms[face]);
  const mat3x3f m(n);
  float l = 0.f;
  loopi(2) loopj(2) {
    const vec3f u = (i*0.5f-0.5f)*m.vx;
    const vec3f v = (j*0.5f-0.5f)*m.vz;
    const ray r(pos+n+u+v, ldir);
    const auto res = world::castray(r);
    l += res.isec ? 0.5f : 1.f;
  }
  return vec3f(l) / 4.f;
#endif
  return vec3f(one);
}

INLINE void buildfaces(brickmeshctx &ctx, vec3i xyz, vec3i idx) {
  const int chan = ctx.face/2; // basically: x (0), y (1) or z (2)
  if (world::getcube(xyz).mat == world::EMPTY) // nothing here
    return;
  if (world::getcube(xyz+cubenorms[ctx.face]).mat != world::EMPTY) // invisible face
    return;

  // build both triangles. we reuse already output vertices
  const int idx0 = 2*ctx.face+0, idx1 = 2*ctx.face+1;
  const vec3i tris[] = {cubetris[idx0], cubetris[idx1]};
  const auto tex = world::getcube(xyz).tex[ctx.face];
  loopi(2) { // build both triangles
    vec3f v[3]={zero,zero,zero}; // delay vertex creation for degenerated tris
    vec2f t[3]={zero,zero,zero}; // idem for texture coordinates
    vec3f c[3]={zero,zero,zero}; // idem for vertex colors
    vec3i locals[3]={zero,zero,zero}; 
    bool isnew[3]={false,false,false};
    loopj(3) { // build each vertex
      const vec3i global = xyz+cubeiverts[tris[i][j]];
      locals[j] = idx+cubeiverts[tris[i][j]];
      u16 id = ctx.get(locals[j]);
      if (id == 0xffff) {
        const vec3f pos = world::getpos(global);
        const vec2f tex = chan==0?pos.yz():(chan==1?pos.xz():pos.xy());
        id = ctx.vbo.length();
        v[j] = pos.xzy();
        t[j] = tex;
        c[j] = computecolor(pos, ctx.face);
        isnew[j] = true;
      } else
        v[j] = vec3f(ctx.vbo[id][0],ctx.vbo[id][1],ctx.vbo[id][2]);
    }
    const float mindist = 1.f/512.f;
    if (distance(v[0], v[1]) < mindist || // degenerated triangle?
        distance(v[1], v[2]) < mindist ||
        distance(v[2], v[0]) < mindist)
      continue;
    else loopj(3) {
      if (isnew[j]) {
        ctx.set(locals[j], ctx.vbo.length());
        //ctx.vbo.add(arrayf<5>(v[j],t[j]));
        ctx.vbo.add(arrayf<8>(v[j],t[j],c[j]));
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
  if (b.dirty==0) return;
  b.dirty = 0;
  brickmeshctx ctx(b);
  loopi(6) {
    ctx.clear(i);
    loopxyz(0, b.size(), buildfaces(ctx, org+xyz, xyz));
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
  OGL(BufferData, GL_ARRAY_BUFFER, ctx.vbo.length()*sizeof(arrayf<8>), &ctx.vbo[0][0], GL_STATIC_DRAW);
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

void buildgrid(void) { forallbricks(buildgridmesh); }
COMMAND(buildgrid, ARG_NONE);

static void drawgrid(void) {
  using namespace world;
  ogl::bindtexture(GL_TEXTURE_2D, ogl::lookuptex(0));
#if 0
  ogl::enableattribarrayv(ogl::POS0, ogl::TEX);
  ogl::disableattribarrayv(ogl::COL, ogl::POS1);
#else
  ogl::enableattribarrayv(ogl::POS0, ogl::TEX, ogl::COL);
  ogl::disableattribarrayv(ogl::POS1);
#endif
  ogl::bindshader(ogl::DIFFUSETEX|ogl::COLOR_ONLY);
  forallbricks([&](const lvl1grid &b, const vec3i org) {
    ogl::bindbuffer(ogl::ARRAY_BUFFER, b.vbo);
    ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, b.ibo);
    OGL(VertexAttribPointer, ogl::COL, 3, GL_FLOAT, 0, sizeof(float[8]), (const void*) sizeof(float[5]));
    OGL(VertexAttribPointer, ogl::TEX, 2, GL_FLOAT, 0, sizeof(float[8]), (const void*) sizeof(float[3]));
    OGL(VertexAttribPointer, ogl::POS0, 3, GL_FLOAT, 0, sizeof(float[8]), (const void*) 0);
    u32 offset = 0;
    loopi(b.draws.length()) {
      const auto fake = (const void*)(uintptr_t(offset*sizeof(u16)));
      const u32 n = b.draws[i].x;
      const u32 tex = b.draws[i].y;
      ogl::bindtexture(GL_TEXTURE_2D, ogl::lookuptex(tex));
      ogl::drawelements(GL_TRIANGLES, n, GL_UNSIGNED_SHORT, fake);
      ogl::xtraverts += n;
      offset += n;
    }
  });
}

/*--------------------------------------------------------------------------
 - render md2 model
 -------------------------------------------------------------------------*/
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
  for (u32 i = 0; i < sizeof(generatedids) / sizeof(generatedids[0]); ++i)
    generatedids[i] = 0;
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
  drawgrid();
  disablev(GL_CULL_FACE);

  drawhudgun(fovy, aspect, farplane);
  if (world::raycast) {
    world::castray(fovy, aspect, farplane);
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

