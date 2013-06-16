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
struct memnode {
  memnode *prev, *next;
};
static memnode memroot = {&memroot, &memroot};
struct DEFAULT_ALIGNED memblock : memnode {
  const char *file;
  u32 linenum;
  u32 allocnum;
  u32 size;
  u32 bound;
  INLINE u32 &rbound(void) {return (&bound)[size/sizeof(u32)+1];}
  INLINE u32 &lbound(void) {return bound;}
};
static SDL_mutex *memmutex = NULL;
static u32 memallocnum = 0;
static bool memfirstalloc = true;

static void meminitblock(memblock *block, size_t sz, const char *file, int linenum) {
  block->file = file;
  block->linenum = linenum;
  block->allocnum = 0;
  block->size = sz;
  block->next = block->prev = NULL;
  block->rbound() = block->lbound() = 0xdeadc0de;
}
static void memlinknode(memnode *node, memnode *next) {
  if (memmutex) SDL_LockMutex(memmutex);
  ((memblock*)node)->allocnum = memallocnum++;
  node->prev = next->prev;
  node->prev->next = node;
  next->prev = node;
  node->next = next;
  if (memmutex) SDL_UnlockMutex(memmutex);
}
static void memunlinknode(memnode *node) {
  if (memmutex) SDL_LockMutex(memmutex);
  ((memblock*)node)->allocnum = memallocnum++;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->next = node->prev = node;
  if (memmutex) SDL_UnlockMutex(memmutex);
}

#define MEMALLOC(S,B)\
  sprintf_sd(S)("file: %s, line %i, size %i bytes, alloc, %i", (B)->file, (B)->linenum, (B)->size, (B)->allocnum)

static void memcheckbounds(memblock *node) {
  if (node->lbound() != 0xdeadc0de || node->rbound() != 0xdeadc0de) {
    fprintf(stderr, "memory corruption detected (alloc %i)\n", node->allocnum);
    DEBUGBREAK;
    _exit(EXIT_FAILURE);
  }
}
static void memoutputalloc(void) {
  memnode *node = memroot.next;
  size_t sz = 0;
  while (node != &memroot) {
    MEMALLOC(unfreed, (memblock*)node);
    fprintf(stderr, "unfreed allocation: %s\n", unfreed);
    sz += ((memblock*)node)->size;
    node = node->next;
  }
  fprintf(stderr, "total unfreed: %fKB \n", float(sz)/1000.f);
}
static void memonfirstalloc(void) {
  if (memfirstalloc) {
    atexit(memoutputalloc);
    memfirstalloc = false;
  }
}
#undef MEMALLOC

void meminit(void) { memmutex = SDL_CreateMutex(); }
void *memalloc(size_t sz, const char *filename, int linenum) {
  memonfirstalloc();
  if (sz) {
    sz = ALIGN(sz, DEFAULT_ALIGNMENT);
    auto block = (memblock*) malloc(sz+sizeof(memblock)+sizeof(u32));
    meminitblock(block, sz, filename, linenum);
    memlinknode(block, &memroot);
    return (void*) (block+1);
  } else
    return NULL;
}

void memfree(void *ptr) {
  memonfirstalloc();
  if (ptr) {
    auto block = (memblock*)((char*)ptr-sizeof(memblock));
    memcheckbounds(block);
    memunlinknode(block);
    free(block);
  }
}

void *memrealloc(void *ptr, size_t sz, const char *filename, int linenum) {
  memonfirstalloc();
  auto block = (memblock*)((char*)ptr-sizeof(memblock));
  if (ptr) {
    memcheckbounds(block);
    memunlinknode(block);
  }
  if (sz) {
    sz = ALIGN(sz, DEFAULT_ALIGNMENT);
    block = (memblock*) realloc(block, sz+sizeof(memblock)+sizeof(u32));
    meminitblock(block, sz, filename, linenum);
    memlinknode(block, &memroot);
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

