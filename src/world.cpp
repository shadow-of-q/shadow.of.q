#include "cube.hpp"
#include <SDL/SDL.h>
#include <zlib.h>
#include "bvh.hpp"

namespace cube {
namespace world {

using namespace game;
const int ssize = 1024;

int findentity(int type, int index) {
  for (int i = index; i<ents.length(); i++)
    if (ents[i].type==type) return i;
  loopj(index) if (ents[j].type==type) return j;
  return -1;
}

// map file format header
static struct header {
  char head[4]; // "CUBE"
  int version; // any >8bit quantity is a little endian
  int headersize; // sizeof(header)
  int sfactor; // in bits
  int numents;
  char maptitle[128];
  u8 texlists[3][256];
  int waterlevel;
  int reserved[15];
} hdr;

int waterlevel(void) { return hdr.waterlevel; }
char *maptitle(void) { return hdr.maptitle; }

void trigger(int tag, int type, bool savegame) {
  if (!tag) return;
  // settag(tag, type);
  if (!savegame && type!=3)
    sound::play(sound::RUMBLE);
  sprintf_sd(aliasname)("level_trigger_%d", tag);
  if (cmd::identexists(aliasname))
    cmd::execute(aliasname);
  if (type==2)
   endsp(false);
}

int closestent(void) {
  if (edit::noteditmode()) return -1;
  int best = 0;
  float bdist = 99999.f;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type==NOTUSED) continue;
    const vec3f v(float(e.x), float(e.y), float(e.z));
    const float dist = distance(player1->o, v);
    if (dist<bdist) {
      best = i;
      bdist = dist;
    }
  }
  return bdist==99999.f ? -1 : best;
}

void entproperty(int prop, int amount) {
  const int e = closestent();
  if (e < 0) return;
  switch (prop) {
    case 0: ents[e].attr1 += amount; break;
    case 1: ents[e].attr2 += amount; break;
    case 2: ents[e].attr3 += amount; break;
    case 3: ents[e].attr4 += amount; break;
  }
}

static void delent(void) {
  const int e = closestent();
  if (e < 0) {
    console::out("no more entities");
    return;
  }
  const int t = ents[e].type;
  console::out("%s entity deleted", entnames(t));
  ents[e].type = NOTUSED;
  client::addmsg(1, 10, SV_EDITENT, e, NOTUSED, 0, 0, 0, 0, 0, 0, 0);
}
COMMAND(delent, ARG_NONE);

static int findtype(const char *what) {
  loopi(MAXENTTYPES) if (strcmp(what,entnames(i))==0) return i;
  console::out("unknown entity type \"%s\"", what);
  return NOTUSED;
}

entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4) {
  const int type = findtype(what);
  persistent_entity e = {short(x), short(y), short(z), short(v1),
    u8(type), u8(v2), u8(v3), u8(v4)};
  switch (type) {
    case LIGHT:
      if (v1>32) v1 = 32;
      if (!v1) e.attr1 = 16;
      if (!v2 && !v3 && !v4) e.attr2 = 255;
    break;
    case MAPMODEL:
      e.attr4 = e.attr3;
      e.attr3 = e.attr2;
    case MONSTER:
    case TELEDEST:
      e.attr2 = (u8)e.attr1;
    case PLAYERSTART:
      e.attr1 = (int)player1->yaw;
    break;
  }
  client::addmsg(1, 10, SV_EDITENT, ents.length(), type, e.x, e.y, e.z, e.attr1, e.attr2, e.attr3, e.attr4);
  ents.add(*((entity *)&e)); // unsafe!
  return &ents.last();
}

static void clearents(char *name) {
  int type = findtype(name);
  if (edit::noteditmode() || client::multiplayer())
    return;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type==type) e.type = NOTUSED;
  }
}
COMMAND(clearents, ARG_1STR);

int isoccluded(float vx, float vy, float cx, float cy, float csize) {return 0;}

static string cgzname, bakname, pcfname, mcfname;

struct deletegrid {
  template <typename G> void operator()(G &g, vec3i) const {
    if (uintptr_t(&g)!=uintptr_t(&root)) {
      auto ptr= &g;
	  SAFE_DELETE(ptr);
	}
  }
};
static void empty(void) {
  forallgrids(deletegrid());
  MEMZERO(root.elem);
  root.dirty = 1;
}
void clean(void) { empty(); }

void setnames(const char *name) {
  string pakname, mapname;
  const char *slash = strpbrk(name, "/\\");
  if (slash) {
    strn0cpy(pakname, name, slash-name+1);
    strcpy_s(mapname, slash+1);
  } else {
    strcpy_s(pakname, "base");
    strcpy_s(mapname, name);
  }
  sprintf_s(cgzname)("packages/%s/%s.cgz", pakname, mapname);
  sprintf_s(bakname)("packages/%s/%s_%d.BAK", pakname, mapname, lastmillis());
  sprintf_s(pcfname)("packages/%s/package.cfg", pakname);
  sprintf_s(mcfname)("packages/%s/%s.cfg", pakname, mapname);
  path(cgzname);
  path(bakname);
}

void backup(char *name, char *backupname) {
  remove(backupname);
  rename(name, backupname);
}

void save(const char *mname) {
  if (!*mname) mname = getclientmap();
  setnames(mname);
  backup(cgzname, bakname);
  gzFile f = gzopen(cgzname, "wb9");
  if (!f) {
    console::out("could not write map to %s", cgzname);
    return;
  }
  hdr.version = MAPVERSION;
  hdr.numents = 0;
  loopv(ents) if (ents[i].type!=NOTUSED) hdr.numents++;
  header tmp = hdr;
  endianswap(&tmp.version, sizeof(int), 4);
  endianswap(&tmp.waterlevel, sizeof(int), 16);
  gzwrite(f, &tmp, sizeof(header));
  loopv(ents) {
    if (ents[i].type!=NOTUSED) {
      entity tmp = ents[i];
      endianswap(&tmp, sizeof(short), 4);
      gzwrite(f, &tmp, sizeof(persistent_entity));
    }
  }
  s32 n=0;
  forallcubes([&](const brickcube&, const vec3i&){++n;});
  gzwrite(f, &n, sizeof(n));
  vec3i pos(zero);
  forallcubes([&](const brickcube &c, const vec3i &xyz) {
    vec3i delta = xyz-pos;
    gzwrite(f, (void*) &delta, sizeof(vec3i));
    pos = xyz;
  });
  vec3i d(zero);
  forallcubes([&](const brickcube &c, const vec3i &xyz) {
    vec3i delta = vec3i(c.p) - d;
    gzwrite(f, (void*) &delta, sizeof(vec3i));
    d = vec3i(c.p);
  });
  s16 mat = 0;
  forallcubes([&](const brickcube &c, const vec3i &xyz) {
    s16 delta = c.mat - mat;
    gzwrite(f, (void*) &delta, sizeof(s16));
    mat = c.mat;
  });
  loopi(6) {
    s16 tex = 0;
    forallcubes([&](const brickcube &c, const vec3i &xyz) {
      s16 delta = c.tex[i] - tex;
      gzwrite(f, (void*) &delta, sizeof(s16));
      tex = c.tex[i];
    });
  }
  gzclose(f);
  console::out("wrote map file %s (%i cubes in total)", cgzname, n);
}

void load(const char *mname) {
  demo::stopifrecording();
  edit::pruneundos();
  setnames(mname);
  empty();
  gzFile f = gzopen(cgzname, "rb9");
  if (!f) {
    console::out("could not read map %s", cgzname);
    return;
  }
  gzread(f, &hdr, sizeof(header)-sizeof(int)*16);
  endianswap(&hdr.version, sizeof(int), 4);
  if (strncmp(hdr.head, "CUBE", 4)!=0)
    fatal("while reading map: header malformatted");
  if (hdr.version>MAPVERSION)
    fatal("this map requires a newer version of cube");
  if (hdr.sfactor<SMALLEST_FACTOR || hdr.sfactor>LARGEST_FACTOR)
    fatal("illegal map size");
  if (hdr.version>=4) {
    gzread(f, &hdr.waterlevel, sizeof(int)*16);
    endianswap(&hdr.waterlevel, sizeof(int), 16);
  } else
    hdr.waterlevel = -100000;
  ents.setsize(0);
  loopi(hdr.numents) {
    entity &e = ents.add();
    gzread(f, &e, sizeof(persistent_entity));
    endianswap(&e, sizeof(short), 4);
    e.spawned = false;
    if (e.type==LIGHT) {
      if (!e.attr2) e.attr2 = 255; // needed for MAPVERSION<=2
      if (e.attr1>32) e.attr1 = 32; // 12_03 and below
    }
  }
  s32 n;
  gzread(f, &n, sizeof(n));
  vector<vec3i> xyz(n);
  vector<brickcube> cubes(n);
  vec3i p(zero); // get all world cube positions
  loopi(n) {
    gzread(f, (void*) &xyz[i], sizeof(vec3i));
    xyz[i] += p;
    p = xyz[i];
  }
  vec3i d(zero); // get displacements
  loopi(n) {
    vec3i r;
    gzread(f, (void*) &r, sizeof(vec3i));
    cubes[i].p = vec3<s8>(r+d);
    d = r+d;
  }
  s16 m=0; // get all material IDs
  loopi(n) {
    s16 r;
    gzread(f, (void*) &r, sizeof(s16));
    cubes[i].mat = s8(r+m);
    m += r;
  }
  loopj(6) { // get all textures
    s16 tex = 0;
    loopi(n) {
      gzread(f, (void*) &cubes[i].tex[j], sizeof(s16));
      cubes[i].tex[j] += tex;
      tex = cubes[i].tex[j];
    }
  }
  loopv(xyz) { world::setcube(xyz[i], cubes[i]); }
  gzclose(f);

  console::out("read map %s (%d milliseconds)", cgzname, SDL_GetTicks()-lastmillis());
  console::out("%s", hdr.maptitle);
  startmap(mname);
  loopl(256) {
    sprintf_sd(aliasname)("level_trigger_%d", l);
    if (cmd::identexists(aliasname))
      cmd::alias(aliasname, "");
  }
  cmd::execfile("data/default_map_settings.cfg");
  //cmd::execfile(pcfname);
  //cmd::execfile(mcfname);
}
COMMANDN(savemap, save, ARG_1STR);

lvl3grid root;

brickcube getcube(const vec3i &xyz) {return world::root.get(xyz);}
void setcube(const vec3i &xyz, const brickcube &cube) {
  root.set(xyz, cube);
  const vec3i m = xyz % brickisize;
  loopi(3) // reset neighbors to force rebuild of neighbor bricks
    if (m[i] == brickisize.x-1)
      root.set(xyz+iaxis[i], root.get(xyz+iaxis[i]));
    else if (m[i] == 0)
      root.set(xyz-iaxis[i], root.get(xyz-iaxis[i]));
}

VAR(raycast, 0, 0, 1);

template <typename T> struct gridpolicy {
  static INLINE vec3f cellorg(vec3f boxorg, vec3i xyz, vec3f cellsize) {
    return boxorg+vec3f(xyz)*cellsize;
  }
};
template <> struct gridpolicy<lvl1grid> {
  static INLINE vec3f cellorg(vec3f boxorg, vec3i xyz, vec3f cellsize) {
    return vec3f(zero);
  }
};
INLINE isecres intersect(const brickcube &cube, const vec3f&, const ray&, float t) {
  return isecres(cube.mat==FULL, t);
}

template <typename G>
INLINE isecres intersect(const G *grid, const vec3f &boxorg, const ray &ray, float t) {
  if (grid == NULL) return isecres(false);
  const vec3b signs = ray.dir > vec3f(zero);
  const vec3f cellsize = grid->subcuben();
  const vec3i step = select(signs, vec3i(one), -vec3i(one));
  const vec3i out = select(signs, grid->local(), -vec3i(one));
  const vec3f delta = abs(ray.rdir*cellsize);
  const vec3f entry = ray.org+t*ray.dir;
  vec3i xyz = min(vec3i((entry-boxorg)/cellsize), grid->local()-vec3i(one));
  const vec3f floorentry = vec3f(xyz)*cellsize+boxorg;
  const vec3f exit = floorentry + select(signs, cellsize, vec3f(zero));
  vec3f tmax = vec3f(t)+(exit-entry)*ray.rdir;
  tmax = select(ray.dir==vec3f(zero),vec3f(FLT_MAX),tmax);

  for (;;) {
    const vec3f cellorg = gridpolicy<G>::cellorg(boxorg, xyz, cellsize);
    const auto isec = intersect(grid->fastsubgrid(xyz), cellorg, ray, t);
    if (isec.isec) return isec;
    if (tmax.x < tmax.y) {
      if (tmax.x < tmax.z) {
        xyz.x += step.x;
        if (xyz.x == out.x) return isecres(false);
        t = tmax.x;
        tmax.x += delta.x;
      } else {
        xyz.z += step.z;
        if (xyz.z == out.z) return isecres(false);
        t = tmax.z;
        tmax.z += delta.z;
      }
    } else {
      if (tmax.y < tmax.z) {
        xyz.y += step.y;
        if (xyz.y == out.y) return isecres(false);
        t = tmax.y;
        tmax.y += delta.y;
      } else {
        xyz.z += step.z;
        if (xyz.z == out.z) return isecres(false);
        t = tmax.z;
        tmax.z += delta.z;
      }
    }
  }
  return isecres(false);
}

isecres castray(const ray &ray) {
  const vec3f cellsize(one), boxorg(zero);
  const aabb box(boxorg, cellsize*vec3f(root.global()));
  const isecres res = slab(box, ray.org, ray.rdir, ray.tfar);
  if (!res.isec) return res;
  return intersect(&root, box.pmin, ray, res.t);
}

VAR(usebvh,0,0,1);

bvh::intersector *buildbvh(void) {
  console::out("bvh: starting to build data structure");

  // prepare all triangles
  auto start = SDL_GetTicks();
  vector<bvh::hybrid> bvhprims;
  u32 boxnum = 0, trinum = 0;
  const auto addcube = [&] (const brickcube &c, vec3i xyz) {
    if (c.mat == EMPTY) return;

    // figure out if the cube is visible and which faces are visible
    bool visible[6], anyvisible = false;
    loopk(6) {
      visible[k] = getcube(xyz+cubenorms[k]).mat == world::EMPTY;
      if (visible[k]) anyvisible = true;
    }
    if (!anyvisible) return;

    // figure out if the cube is unchanged (i.e. not deformed)
    vec3f vertices[8];
    bool anydeformed = false;
    loopk(8) {
      const auto &c = world::getcube(xyz+cubeiverts[k]);
      vertices[k] = vec3f(c.p)/255.f;
      if (any(c.p != vec3<s8>(zero)))
        anydeformed = true;
    }

    // we have a deformed cube. we must output the individual faces
    if (anydeformed) loopk(6) {
      if (!visible[k]) continue;
      const int idx0 = 2*k+0, idx1 = 2*k+1;
      const vec3i tris[] = {cubetris[idx0], cubetris[idx1]};
      loopi(2) {
        vec3f v[3];
        loopj(3) {
          const auto global = xyz+cubeiverts[tris[i][j]];
          v[j] = vec3f(global)+vertices[tris[i][j]];
        }
        trinum++;
        bvhprims.add(bvh::hybrid(v[0],v[1],v[2]));
      }
    } else {
      boxnum++;
      bvhprims.add(bvh::hybrid(aabb(vec3f(xyz), vec3f(xyz)+vec3f(one))));
    }
  };
  forallcubes(addcube);
  console::out("bvh: %i generated primitives with %i boxes and %i triangles (%i ms elapsed)",
    bvhprims.length(), boxnum, trinum, SDL_GetTicks()-start);

  start = SDL_GetTicks();
  if (bvhprims.length() > 0) {
    auto isec = bvh::create(&bvhprims[0], bvhprims.length());
    console::out("bvh: data structure created (%i ms elapsed)", SDL_GetTicks()-start);
    return isec;
  } else
    return NULL;
}

void castray(float fovy, float aspect, float farplane) {
  using namespace game;
  const int w = 1024, h = 768;
  int *pixels = (int*)MALLOC(w*h*sizeof(int));
  const mat3x3f r = mat3x3f::rotate(vec3f(0.f,0.f,1.f),game::player1->yaw)*
                    mat3x3f::rotate(vec3f(0.f,1.f,0.f),game::player1->roll)*
                    mat3x3f::rotate(vec3f(-1.f,0.f,0.f),game::player1->pitch);
  const camera cam(game::player1->o, -r.vz, -r.vy, fovy, aspect);
  const vec3f cellsize(one), boxorg(zero);
  const aabb box(boxorg, cellsize*vec3f(root.global()));
  bvh::intersector *bvhisec = NULL;
  if (usebvh) bvhisec = buildbvh();

  const int start = SDL_GetTicks();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int offset = x+w*y;
      const ray ray = cam.generate(w, h, x, y);
      if (bvhisec) {
        bvh::hit hit;
        closest(*bvhisec, ray, hit);
        if (hit.is_hit()) {
          const int d = min(int(hit.t), 255);
          pixels[offset] = d|(d<<8)|(d<<16)|(0xff<<24);
        } else
          pixels[offset] = 0;
      } else {
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
    }
  }
  const int ms = SDL_GetTicks()-start;
  console::out("\n%i ms, %f ray/s\n", ms, 1000.f*(w*h)/ms);
  writebmp(pixels, w, h, usebvh ? "bvh.bmp" : "grid.bmp");
  FREE(pixels);
  if (bvhisec) bvh::destroy(bvhisec);
}

} // namespace world
} // namespace cube

