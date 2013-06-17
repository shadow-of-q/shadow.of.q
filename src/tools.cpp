#include "tools.hpp"
#include "stl.hpp"
#if defined(MEMORY_DEBUGGER)
#include <SDL/SDL_mutex.h>
#endif // defined(MEMORY_DEBUGGER)
#include <cstdio>

#if !defined(__MSVC__)
#include "unistd.h"
#endif

namespace cube {

#if defined(MEMORY_DEBUGGER)
struct DEFAULT_ALIGNED memblock : intrusive_list_node {
  INLINE memblock(size_t sz, const char *file, int linenum) :
    file(file), linenum(linenum), allocnum(0), size(sz)
  {rbound() = lbound() = 0xdeadc0de;}
  const char *file;
  u32 linenum, allocnum, size;
  u32 bound;
  INLINE u32 &rbound(void) {return (&bound)[size/sizeof(u32)+1];}
  INLINE u32 &lbound(void) {return bound;}
};

static intrusive_list<memblock> *memlist = NULL;
static SDL_mutex *memmutex = NULL;
static u32 memallocnum = 0;
static bool memfirstalloc = true;

static void memlinkblock(memblock *node) {
  if (memmutex) SDL_LockMutex(memmutex);
  node->allocnum = memallocnum++;
  memlist->push_back(node);
  if (memmutex) SDL_UnlockMutex(memmutex);
}
static void memunlinkblock(memblock *node) {
  if (memmutex) SDL_LockMutex(memmutex);
  unlink(node);
  if (memmutex) SDL_UnlockMutex(memmutex);
}

#define MEMOUT(S,B)\
  sprintf_sd(S)("file: %s, line %i, size %i bytes, alloc, %i",\
                (B)->file, (B)->linenum, (B)->size, (B)->allocnum)
static void memcheckbounds(memblock *node) {
  if (node->lbound() != 0xdeadc0de || node->rbound() != 0xdeadc0de) {
    fprintf(stderr, "memory corruption detected (alloc %i)\n", node->allocnum);
    DEBUGBREAK;
    _exit(EXIT_FAILURE);
  }
}
static void memoutputalloc(void) {
  size_t sz = 0;
  if (memlist != NULL) {
    for (auto it = memlist->begin(); it != memlist->end(); ++it) {
      MEMOUT(unfreed, it);
      fprintf(stderr, "unfreed allocation: %s\n", unfreed);
      sz += it->size;
    }
    if (sz > 0) fprintf(stderr, "total unfreed: %fKB \n", float(sz)/1000.f);
    delete memlist; 
  }
}

static void memonfirstalloc(void) {
  if (memfirstalloc) {
    memlist = new intrusive_list<memblock>();
    atexit(memoutputalloc);
    memfirstalloc = false;
  }
}
#undef MEMOUT

void meminit(void) { memmutex = SDL_CreateMutex(); }
void *memalloc(size_t sz, const char *filename, int linenum) {
  memonfirstalloc();
  if (sz) {
    sz = ALIGN(sz, DEFAULT_ALIGNMENT);
    auto ptr = malloc(sz+sizeof(memblock)+sizeof(u32));
    auto block = new (ptr) memblock(sz, filename, linenum);
    memlinkblock(block);
    return (void*) (block+1);
  } else
    return NULL;
}

void memfree(void *ptr) {
  memonfirstalloc();
  if (ptr) {
    auto block = (memblock*)((char*)ptr-sizeof(memblock));
    memcheckbounds(block);
    memunlinkblock(block);
    free(block);
  }
}

void *memrealloc(void *ptr, size_t sz, const char *filename, int linenum) {
  memonfirstalloc();
  auto block = (memblock*)((char*)ptr-sizeof(memblock));
  if (ptr) {
    memcheckbounds(block);
    memunlinkblock(block);
  }
  if (sz) {
    sz = ALIGN(sz, DEFAULT_ALIGNMENT);
    auto ptr = realloc(block, sz+sizeof(memblock)+sizeof(u32));
    block = new (ptr) memblock(sz, filename, linenum);
    memlinkblock(block);
    return (void*) (block+1);
  } else if (ptr)
    free(block);
  return NULL;
}

#else
void meminit(void) {}
void *memalloc(size_t sz, const char*, int) {return malloc(sz);}
void *memrealloc(void *ptr, size_t sz, const char *, int) {return realloc(ptr,sz);}
void memfree(void *ptr) {free(ptr);}
#endif // defined(MEMORY_DEBUGGER)

void writebmp(const int *data, int w, int h, const char *filename) {
  int x, y;
  FILE *fp = fopen(filename, "wb");
  ASSERT(fp);
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
  char *raw = (char *) MALLOC(w*h*sizeof(int));
  ASSERT(raw);
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
  ASSERT(sizeraw == scanline * h);

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
  FREE(raw);
}
} // namespace cube

