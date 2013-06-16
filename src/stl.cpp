#include "stl.hpp"
#include <cstdio>

namespace cube {
static int islittleendian_ = 1;
void initendiancheck(void) { islittleendian_ = *((char *)&islittleendian_); }
int islittleendian(void) { return islittleendian_; }

char *newstring(const char *s, size_t l, const char *filename, int linenum) {
  char *b = (char*) memalloc(l+1, filename, linenum);
  strncpy(b,s,l);
  b[l] = 0;
  return b;
}
char *newstring(const char *s, const char *filename, int linenum) {
  return newstring(s, strlen(s), filename, linenum);
}
char *newstringbuf(const char *s, const char *filename, int linenum) {
  return newstring(s, _MAXDEFSTR-1, filename, linenum);
}
void formatstring(char *d, const char *fmt, va_list v) {
  _vsnprintf(d, _MAXDEFSTR, fmt, v);
  d[_MAXDEFSTR-1] = 0;
}
void sprintf_s_f::operator()(const char* fmt, ...) {
  va_list v;
  va_start(v, fmt);
  _vsnprintf(d, _MAXDEFSTR, fmt, v);
  va_end(v);
  d[_MAXDEFSTR-1] = 0;
}

char *path(char *s) {
  for (char *t = s; (t = strpbrk(t, "/\\")) != 0; *t++ = PATHDIV);
  return s;
}

char *loadfile(char *fn, int *size) {
  FILE *f = fopen(fn, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  unsigned int len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)MALLOC(len+1);
  if (!buf) return NULL;
  buf[len] = 0;
  size_t rlen = fread(buf, 1, len, f);
  fclose(f);
  if (len!=rlen || len<=0) {
    FREE(buf);
    return NULL;
  }
  if (size!=NULL) *size = len;
  return buf;
}

void endianswap(void *memory, int stride, int length) {
  if (*((char *)&stride)) return;
  loop(w, length) loop(i, stride/2) {
    u8 *p = (u8 *)memory+w*stride;
    u8 t = p[i];
    p[i] = p[stride-i-1];
    p[stride-i-1] = t;
  }
}
} // namespace cube

