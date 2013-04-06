#ifndef __CUBE_OGL_HPP__
#define __CUBE_OGL_HPP__

#if defined(EMSCRIPTEN)
#include "GLES2/gl2.h"
#else
#include "GL/gl3.h"
#endif

namespace cube {
namespace ogl {

/* We instantiate all GL functions here */
#if !defined(EMSCRIPTEN)
#define GL_PROC(FIELD,NAME,PROTOTYPE) extern PROTOTYPE FIELD;
#include "GL/ogl100.hxx"
#include "GL/ogl110.hxx"
#include "GL/ogl120.hxx"
#include "GL/ogl130.hxx"
#include "GL/ogl150.hxx"
#include "GL/ogl200.hxx"
#include "GL/ogl300.hxx"
#undef GL_PROC
#endif /* EMSCRIPTEN */

/* vertex attributes */
enum {POS0, POS1, TEX, NOR, COL, ATTRIB_NUM};

/* quick, dirty and super simple uber-shader system */
static const uint COLOR_ONLY = 0;
static const uint FOG = 1<<0;
static const uint KEYFRAME = 1<<1;
static const uint DIFFUSETEX = 1<<2;
static const int subtypen = 3;
static const int shadern = 1<<subtypen;
void bindshader(uint flags);

void init(int w, int h);
void clean(void);
void drawframe(int w, int h, float curfps);
bool installtex(int id, const char *name, int &xs, int &ys, bool clamp = false);
int lookuptex(int tex, int &xs, int &ys);

/* draw helper functions */
void draw(int mode, int pos, int tex, size_t n, const float *data);
void drawarrays(int mode, int first, int count);
void drawelements(int mode, int count, int type, const void *indices);
void rendermd2(const float *pos0, const float *pos1, float lerp, int n);
void drawsphere(void);

/* Ensure state tracking */
enum {ARRAY_BUFFER=0,ELEMENT_ARRAY_BUFFER,BUFFER_NUM};
void bindbuffer(int target, uint buffer);
void bindtexture(int target, uint tex);
void enableattribarray(uint target);
void disableattribarray(uint target);

/* immediate mode rendering */
void immvertices(int sz, const void *data);
void immattrib(int attrib, int n, int type, int sz, int offset);
void immdrawelements(int mode, int count, int type, const void *indices);
void immdrawarrays(int mode, int first, int count);
void immdraw(int mode, int pos, int tex, int col, size_t n, const float *data);

/* matrix interface (mostly OGL like) */
enum {MODELVIEW=0, PROJECTION=1, MATRIX_MODE=2};
void matrixmode(int mode);
void identity(void);
void rotate(float angle, const vec3f &axis);
void perspective(const mat4x4f &m, float fovy, float aspect, float znear, float zfar);
void translate(const vec3f &v);
void mulmatrix(const mat4x4f &m);
void pushmatrix(void);
void popmatrix(void);
void loadmatrix(const mat4x4f &m);
void ortho(float left, float right, float bottom, float top, float znear, float zfar);
void scale(const vec3f &s);
const mat4x4f &matrix(int mode);

/* number of transformed vertices per frame */
extern int xtraverts;

/* OGL debug macros */
#if !defined(EMSCRIPTEN)
#if !defined(NDEBUG)
#define OGL(NAME, ...) \
  do { \
    cube::ogl::NAME(__VA_ARGS__); \
    if (cube::ogl::GetError()) fatal("gl" #NAME " failed"); \
  } while (0)
#define OGLR(RET, NAME, ...) \
  do { \
    RET = cube::ogl::NAME(__VA_ARGS__); \
    if (cube::ogl::GetError()) fatal("gl" #NAME " failed"); \
  } while (0)
#else
  #define OGL(NAME, ...) do {cube::ogl::NAME(__VA_ARGS__);} while(0)
  #define OGLR(RET, NAME, ...) do {RET=cube::ogl::NAME(__VA_ARGS__);} while(0)
#endif /* NDEBUG */
#else /* EMSCRIPTEN */
#if !defined(NDEBUG)
#define OGL(NAME, ...) \
  do { \
    gl##NAME(__VA_ARGS__); \
    if (gl##GetError()) { \
      char error[2048]; \
      snprintf(error, sizeof(error), "gl" #NAME " failed at line %i and file %s",__LINE__, __FILE__);\
      fatal(error); \
    } \
  } while (0)
#define OGLR(RET, NAME, ...) \
  do { \
    RET = gl##NAME(__VA_ARGS__); \
    if (gl##GetError()) fatal("gl" #NAME " failed"); \
  } while (0)
#else
  #define OGL(NAME, ...) do {gl##NAME(__VA_ARGS__);} while(0)
  #define OGLR(RET, NAME, ...) do {RET=gl##NAME(__VA_ARGS__);} while(0)
#endif /* NDEBUG */
#endif /* EMSCRIPTEN */

} /* namespace ogl */
} /* namespace cube */

#endif /* __CUBE_OGL_HPP__ */

