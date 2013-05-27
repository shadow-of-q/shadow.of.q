#include "tools.hpp"
#include <new>

namespace cube {

static int islittleendian_ = 1;
void initendiancheck(void) { islittleendian_ = *((char *)&islittleendian_); }
int islittleendian(void) { return islittleendian_; }

pool::pool(void) {
  blocks = 0;
  allocnext(POOLSIZE);
  for (int i = 0; i<MAXBUCKETS; i++) reuse[i] = NULL;
}

void *pool::alloc(size_t size) {
  if (size>MAXREUSESIZE)
    return malloc(size);
  else {
    size = bucket(size);
    void **r = (void **)reuse[size];
    if (r) {
      reuse[size] = *r;
      return (void *)r;
    } else {
      size <<= PTRBITS;
      if (left<size) allocnext(POOLSIZE);
      char *r = p;
      p += size;
      left -= size;
      return r;
    }
  }
}

void pool::dealloc(void *p, size_t size) {
  if (size>MAXREUSESIZE)
    free(p);
  else {
    size = bucket(size);
    if (size) {    // only needed for 0-size free, are there any?
      *((void **)p) = reuse[size];
      reuse[size] = p;
    }
  }
}

void *pool::realloc(void *p, size_t oldsize, size_t newsize) {
  void *np = alloc(newsize);
  if (!oldsize) return np;
  if (oldsize) {
    memcpy(np, p, newsize>oldsize ? oldsize : newsize);
    dealloc(p, oldsize);
  }
  return np;
}

void pool::dealloc_block(void *b) {
  if (b) {
    dealloc_block(*((char **)b));
    free(b);
  }
}

void pool::allocnext(size_t allocsize) {
  char *b = (char *)malloc(allocsize+PTRSIZE);
  *((char **)b) = blocks;
  blocks = b;
  p = b+PTRSIZE;
  left = allocsize;
}

char *pool::string(const char *s, size_t l) {
  char *b = (char *)alloc(l+1);
  strncpy(b,s,l);
  b[l] = 0;
  return b;
}

/* useful for global buffers that need to be initialisation
 * order independant
 */
pool *gp(void) {
  static pool *p = NULL;
  return p ? p : (p = new pool());
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
  char *buf = (char *)malloc(len+1);
  if (!buf) return NULL;
  buf[len] = 0;
  size_t rlen = fread(buf, 1, len, f);
  fclose(f);
  if (len!=rlen || len<=0) {
    free(buf);
    return NULL;
  }
  if (size!=NULL) *size = len;
  return buf;
}

// little indians as storage format
void endianswap(void *memory, int stride, int length) {
  if (*((char *)&stride)) return;
  loop(w, length) loop(i, stride/2) {
    uchar *p = (uchar *)memory+w*stride;
    uchar t = p[i];
    p[i] = p[stride-i-1];
    p[stride-i-1] = t;
  }
}

void writebmp(const int *data, int w, int h, const char *filename) {
  int x, y;
  FILE *fp = fopen(filename, "wb");
  assert(fp);
  struct bmphdr {
    int filesize;
    short as0, as1;
    int bmpoffset;
    int headerbytes;
    int w;
    int h;
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
  char *raw = (char *) malloc(w * h * sizeof(int)); // at most
  assert(raw);
  char *p = raw;

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
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
  int scanline = (w * 3 + 3) & ~3;
  assert(sizeraw == scanline * h);

  struct bmphdr hdr;
  hdr.filesize = scanline * h + sizeof(hdr) + 2;
  hdr.as0 = 0;
  hdr.as1 = 0;
  hdr.bmpoffset = sizeof(hdr) + 2;
  hdr.headerbytes = 40;
  hdr.w = w;
  hdr.h = h;
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
} // namespace cube

