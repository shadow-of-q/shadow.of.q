#include "cube.h"
#include "ogl.hpp"
#include <SDL/SDL.h>
#include <zlib.h>

namespace cube {

// lookup from map entities above to strings
extern const char *entnames[];

// XXX move that
const int ssize = 1024;
extern bool hasoverbright;

namespace world {
int findentity(int type, int index)
{
  for (int i = index; i<ents.length(); i++)
    if (ents[i].type==type) return i;
  loopj(index) if (ents[j].type==type) return j;
  return -1;
}

// map file format header
static struct header {
  char head[4]; // "CUBE"
  int version; // any >8bit quantity is a little indian
  int headersize; // sizeof(header)
  int sfactor; // in bits
  int numents;
  char maptitle[128];
  uchar texlists[3][256];
  int waterlevel;
  int reserved[15];
} hdr;

int waterlevel(void) { return hdr.waterlevel; }
char *maptitle(void) { return hdr.maptitle; }

void trigger(int tag, int type, bool savegame)
{
  if (!tag) return;
  // settag(tag, type);
  if (!savegame && type!=3)
    sound::play(S_RUMBLE);
  sprintf_sd(aliasname)("level_trigger_%d", tag);
  if (cmd::identexists(aliasname))
    cmd::execute(aliasname);
  if (type==2)
   monster::endsp(false);
}

int closestent(void)
{
  if (edit::noteditmode()) return -1;
  int best = 0;
  float bdist = 99999;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type==NOTUSED) continue;
    const vec v(float(e.x), float(e.y), float(e.z));
    vdist(dist, t, player1->o, v);
    if (dist<bdist) {
      best = i;
      bdist = dist;
    }
  }
  return bdist==99999 ? -1 : best;
}

void entproperty(int prop, int amount)
{
  const int e = closestent();
  if (e < 0) return;
  switch (prop) {
    case 0: ents[e].attr1 += amount; break;
    case 1: ents[e].attr2 += amount; break;
    case 2: ents[e].attr3 += amount; break;
    case 3: ents[e].attr4 += amount; break;
  }
}

static void delent(void)
{
  const int e = closestent();
  if (e < 0) {
    console::out("no more entities");
    return;
  }
  const int t = ents[e].type;
  console::out("%s entity deleted", entnames[t]);
  ents[e].type = NOTUSED;
  client::addmsg(1, 10, SV_EDITENT, e, NOTUSED, 0, 0, 0, 0, 0, 0, 0);
}
COMMAND(delent, ARG_NONE);

static int findtype(const char *what)
{
  loopi(MAXENTTYPES) if (strcmp(what,entnames[i])==0) return i;
  console::out("unknown entity type \"%s\"", what);
  return NOTUSED;
}

entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4)
{
  const int type = findtype(what);
  persistent_entity e = {short(x), short(y), short(z), short(v1),
    uchar(type), uchar(v2), uchar(v3), uchar(v4)};
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
      e.attr2 = (uchar)e.attr1;
    case PLAYERSTART:
      e.attr1 = (int)player1->yaw;
    break;
  }
  client::addmsg(1, 10, SV_EDITENT, ents.length(), type, e.x, e.y, e.z, e.attr1, e.attr2, e.attr3, e.attr4);
  ents.add(*((entity *)&e)); // unsafe!
  return &ents.last();
}

static void clearents(char *name)
{
  int type = findtype(name);
  if (edit::noteditmode() || client::multiplayer())
    return;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type==type) e.type = NOTUSED;
  }
}
COMMAND(clearents, ARG_1STR);

int isoccluded(float vx, float vy, float cx, float cy, float csize)
{
  return 0;
}

static string cgzname, bakname, pcfname, mcfname;

void setnames(const char *name)
{
  string pakname, mapname;
  const char *slash = strpbrk(name, "/\\");
  if (slash) {
    strn0cpy(pakname, name, slash-name+1);
    strcpy_s(mapname, slash+1);
  } else {
    strcpy_s(pakname, "base");
    strcpy_s(mapname, name);
  }
  sprintf_s(cgzname)("packages/%s/%s.cgz",      pakname, mapname);
  sprintf_s(bakname)("packages/%s/%s_%d.BAK",   pakname, mapname, lastmillis);
  sprintf_s(pcfname)("packages/%s/package.cfg", pakname);
  sprintf_s(mcfname)("packages/%s/%s.cfg",      pakname, mapname);
  path(cgzname);
  path(bakname);
}

void backup(char *name, char *backupname)
{
  remove(backupname);
  rename(name, backupname);
}

void save(const char *mname)
{
  if (!*mname) mname = game::getclientmap();
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
  gzclose(f);
  console::out("wrote map file %s", cgzname);
}

void load(const char *mname)
{
  demo::stopifrecording();
  edit::pruneundos();
  setnames(mname);
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
  gzclose(f);

  int xs, ys;
  loopi(256) ogl::lookuptex(i, xs, ys);
  console::out("read map %s (%d milliseconds)", cgzname, SDL_GetTicks()-lastmillis);
  console::out("%s", hdr.maptitle);
  game::startmap(mname);
  loopl(256) {
    sprintf_sd(aliasname)("level_trigger_%d", l);
    if (cmd::identexists(aliasname))
      cmd::alias(aliasname, "");
  }
  cmd::execfile("data/default_map_settings.cfg");
  cmd::execfile(pcfname);
  cmd::execfile(mcfname);
}
COMMANDN(savemap, save, ARG_1STR);

} // namespace world
} // namespace cube

