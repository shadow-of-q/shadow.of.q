#include "cube.h"
#include <zlib.h>

// loading and saving of savegames & demos, dumps the spawn state of all
// mapents, the full state of all dynents (monsters + player)
namespace cube {

// XXX
extern int islittleendian;

namespace demo {

static gzFile f = NULL;
static dvector playerhistory;
static bool demorecording = false;
static bool demoloading = false;
static bool demoplayback = false;
static int democlientnum = 0;

static void start(void);

static void gzput(int i) { gzputc(f, i); }
static void gzputi(int i) { gzwrite(f, &i, sizeof(int)); }
static void gzputv(vec &v) { gzwrite(f, &v, sizeof(vec)); }
static void gzcheck(int a, int b) {
  if (a!=b) fatal("savegame file corrupt (short)");
}
static int gzget(void) {
  char c = gzgetc(f);
  return c;
}
static int gzgeti(void) {
  int i;
  gzcheck(gzread(f, &i, sizeof(int)), sizeof(int));
  return i;
}
static void gzgetv(vec &v) { gzcheck(gzread(f, &v, sizeof(vec)), sizeof(vec)); }

bool playing(void) { return demoplayback; }
int clientnum(void) { return democlientnum; }

void stop(void) {
  if (f) {
    if (demorecording) gzputi(-1);
    gzclose(f);
  }
  f = NULL;
  demorecording = false;
  demoplayback = false;
  demoloading = false;
  loopv(playerhistory) game::zapdynent(playerhistory[i]);
  playerhistory.setsize(0);
}

void stopifrecording(void) { if (demorecording) stop(); }

static void savestate(char *fn) {
  stop();
  f = gzopen(fn, "wb9");
  if (!f) { console::out("could not write %s", fn); return; }
  gzwrite(f, (void *)"CUBESAVE", 8);
  gzputc(f, islittleendian);
  gzputi(SAVEGAMEVERSION);
  gzputi(sizeof(dynent));
  gzwrite(f, (const voidp) game::getclientmap(), _MAXDEFSTR);
  gzputi(gamemode);
  gzputi(ents.length());
  loopv(ents) gzputc(f, ents[i].spawned);
  gzwrite(f, player1, sizeof(dynent));
  dvector &monsters = monster::getmonsters();
  gzputi(monsters.length());
  loopv(monsters) gzwrite(f, monsters[i], sizeof(dynent));
  gzputi(players.length());
  loopv(players) {
    gzput(players[i]==NULL);
    gzwrite(f, players[i], sizeof(dynent));
  }
}

static void savegame(char *name) {
  if (!m_classicsp) {
    console::out("can only save classic sp games");
    return;
  }
  sprintf_sd(fn)("savegames/%s.csgz", name);
  savestate(fn);
  stop();
  console::out("wrote %s", fn);
}
COMMAND(savegame, ARG_1STR);

static void loadstate(char *fn) {
  stop();
  if (client::multiplayer()) return;
  f = gzopen(fn, "rb9");
  if (!f) { console::out("could not open %s", fn); return; }

  string buf;
  gzread(f, buf, 8);
  if (strncmp(buf, "CUBESAVE", 8)) goto out;
  if (gzgetc(f)!=islittleendian) goto out;     // not supporting save->load accross incompatible architectures simpifies things a LOT
  if (gzgeti()!=SAVEGAMEVERSION || gzgeti()!=sizeof(dynent)) goto out;
  string mapname;
  gzread(f, mapname, _MAXDEFSTR);
  nextmode = gzgeti();
  client::changemap(mapname); // continue below once map has been loaded and client & server have updated
  return;
out:
  console::out("aborting: savegame/demo from a different version of cube or cpu architecture");
  stop();
}

static void loadgame(char *name) {
  sprintf_sd(fn)("savegames/%s.csgz", name);
  loadstate(fn);
}
COMMAND(loadgame, ARG_1STR);

static void loadgameout(void) {
  stop();
  console::out("loadgame incomplete: savegame from a different version of this map");
}

void loadgamerest(void) {
  if (demoplayback || !f) return;
  if (gzgeti()!=ents.length()) return loadgameout();
  loopv(ents) {
    ents[i].spawned = gzgetc(f)!=0;
    if (ents[i].type==CARROT && !ents[i].spawned)
      world::trigger(ents[i].attr1, ents[i].attr2, true);
  }
  server::restoreserverstate(ents);

  gzread(f, player1, sizeof(dynent));
  player1->lastaction = lastmillis;

  int nmonsters = gzgeti();
  dvector &monsters = monster::getmonsters();
  if (nmonsters!=monsters.length()) return loadgameout();
  loopv(monsters) {
    gzread(f, monsters[i], sizeof(dynent));
    monsters[i]->enemy = player1; // lazy, could save id of enemy instead
    monsters[i]->lastaction = monsters[i]->trigger = lastmillis+500; // also lazy, but no real noticable effect on game
    if (monsters[i]->state==CS_DEAD) monsters[i]->lastaction = 0;
  }
  monster::restoremonsterstate();

  int nplayers = gzgeti();
  loopi(nplayers) if (!gzget()) {
    dynent *d = game::getclient(i);
    assert(d);
    gzread(f, d, sizeof(dynent));
  }

  console::out("savegame restored");
  if (demoloading) start(); else stop();
}

// demo functions
static int starttime = 0;
static int playbacktime = 0;
static int ddamage, bdamage;
static vec dorig;

static void record(char *name) {
  if (m_sp) {
    console::out("cannot record singleplayer games");
    return;
  }
  int cn = client::getclientnum();
  if (cn<0) return;
  sprintf_sd(fn)("demos/%s.cdgz", name);
  savestate(fn);
  gzputi(cn);
  console::out("started recording demo to %s", fn);
  demorecording = true;
  starttime = lastmillis;
  ddamage = bdamage = 0;
}
COMMAND(record, ARG_1STR);

void damage(int damage, vec &o) { ddamage = damage; dorig = o; }
void blend(int damage) { bdamage = damage; }

void incomingdata(uchar *buf, int len, bool extras) {
  if (!demorecording) return;
  gzputi(lastmillis-starttime);
  gzputi(len);
  gzwrite(f, buf, len);
  gzput(extras);
  if (extras) {
    gzput(player1->gunselect);
    gzput(player1->lastattackgun);
    gzputi(player1->lastaction-starttime);
    gzputi(player1->gunwait);
    gzputi(player1->health);
    gzputi(player1->armour);
    gzput(player1->armourtype);
    loopi(NUMGUNS) gzput(player1->ammo[i]);
    gzput(player1->state);
    gzputi(bdamage);
    bdamage = 0;
    gzputi(ddamage);
    if (ddamage) { gzputv(dorig); ddamage = 0; }
    // FIXME: add all other client state which is not send through the network
  }
}

static void demo(char *name) {
  sprintf_sd(fn)("demos/%s.cdgz", name);
  loadstate(fn);
  demoloading = true;
}
COMMAND(demo, ARG_1STR);

static void stopreset(void) {
  console::out("demo stopped (%d msec elapsed)", lastmillis-starttime);
  stop();
  loopv(players) game::zapdynent(players[i]);
  client::disconnect(0, 0);
}

VAR(demoplaybackspeed, 10, 100, 1000);
static int scaletime(int t) {
  return (int)(t*(100.0f/demoplaybackspeed))+starttime;
}

static void readdemotime() {
  if (gzeof(f) || (playbacktime = gzgeti())==-1) {
    stopreset();
    return;
  }
  playbacktime = scaletime(playbacktime);
}

static void start(void) {
  democlientnum = gzgeti();
  demoplayback = true;
  starttime = lastmillis;
  console::out("now playing demo");
  dynent *d = game::getclient(democlientnum);
  assert(d);
  *d = *player1;
  readdemotime();
}

VAR(demodelaymsec, 0, 120, 500);

// spline interpolation
static void catmulrom(vec &z, vec &a, vec &b, vec &c, float s, vec &dest) {
  vec t1 = b, t2 = c;
  vsub(t1, z); vmul(t1, 0.5f)
  vsub(t2, a); vmul(t2, 0.5f);

  const float s2 = s*s;
  const float s3 = s*s2;

  dest = a;
  vec t = b;

  vmul(dest, 2*s3 - 3*s2 + 1);
  vmul(t,   -2*s3 + 3*s2);     vadd(dest, t);
  vmul(t1,     s3 - 2*s2 + s); vadd(dest, t1);
  vmul(t2,     s3 -   s2);     vadd(dest, t2);
}

static void fixwrap(dynent *a, dynent *b) {
  while (b->yaw-a->yaw>180)  a->yaw += 360;
  while (b->yaw-a->yaw<-180) a->yaw -= 360;
}

void playbackstep(void) {
  while (demoplayback && lastmillis>=playbacktime) {
    int len = gzgeti();
    if (len<1 || len>MAXTRANS) {
      console::out("error: huge packet during demo play (%d)", len);
      stopreset();
      return;
    }
    uchar buf[MAXTRANS];
    gzread(f, buf, len);
    client::localservertoclient(buf, len);  // update game state

    dynent *target = players[democlientnum];
    assert(target);

    int extras;
    if ((extras = gzget())) { // read additional client side state not present in normal network stream
      target->gunselect = gzget();
      target->lastattackgun = gzget();
      target->lastaction = scaletime(gzgeti());
      target->gunwait = gzgeti();
      target->health = gzgeti();
      target->armour = gzgeti();
      target->armourtype = gzget();
      loopi(NUMGUNS) target->ammo[i] = gzget();
      target->state = gzget();
      target->lastmove = playbacktime;
      if ((bdamage = gzgeti()) != 0) rr::damageblend(bdamage);
      if ((ddamage = gzgeti()) != 0) {
        gzgetv(dorig);
        rr::particle_splash(3, ddamage, 1000, dorig);
      }
      // FIXME: set more client state here
    }

    // insert latest copy of player into history
    if (extras && (playerhistory.empty() || playerhistory.last()->lastupdate!=playbacktime)) {
      dynent *d = game::newdynent();
      *d = *target;
      d->lastupdate = playbacktime;
      playerhistory.add(d);
      if (playerhistory.length()>20) {
        game::zapdynent(playerhistory[0]);
        playerhistory.remove(0);
      }
    }

    readdemotime();
  }

  if (demoplayback) {
    int itime = lastmillis-demodelaymsec;
    loopvrev(playerhistory) if (playerhistory[i]->lastupdate<itime) { // find 2 positions in history that surround interpolation time point
      dynent *a = playerhistory[i];
      dynent *b = a;
      if (i+1<playerhistory.length()) b = playerhistory[i+1];
      *player1 = *b;
      if (a!=b) { // interpolate pos & angles
        dynent *c = b;
        if (i+2<playerhistory.length()) c = playerhistory[i+2];
        dynent *z = a;
        if (i-1>=0) z = playerhistory[i-1];
        float bf = (itime-a->lastupdate)/(float)(b->lastupdate-a->lastupdate);
        fixwrap(a, player1);
        fixwrap(c, player1);
        fixwrap(z, player1);
        vdist(dist, v, z->o, c->o);
        if (dist<16) { // if teleport or spawn, dont't interpolate
          catmulrom(z->o, a->o, b->o, c->o, bf, player1->o);
          catmulrom(*(vec *)&z->yaw, *(vec *)&a->yaw, *(vec *)&b->yaw, *(vec *)&c->yaw, bf, *(vec *)&player1->yaw);
        }
        game::fixplayer1range();
      }
      break;
    }
  }
}

static void stopn(void) {
  if (demoplayback)
    stopreset();
  else
    stop();
  console::out("demo stopped");
}
COMMANDN(stop, stopn, ARG_NONE);

} // namespace demo
} // namespace cube

