#if !defined(STANDALONE)
#include "cube.hpp"
#include <enet/enet.h>

namespace cube {
namespace game {

static int lastmillis_ = 0;
static int nextmode_ = 0;
static vec3f worldpos_;
static int curtime_ = 10;
VAR(gamemode, 1, 0, 0);

GLOBAL_VAR(worldpos, worldpos_, const vec3f&);
GLOBAL_VAR(mode, gamemode, int);
GLOBAL_VAR(nextmode, nextmode_, int);
GLOBAL_VAR(curtime, curtime_, int);
GLOBAL_VAR(lastmillis, lastmillis_, int);
COMMANDN(millis, lastmillis, ARG_1EXP);

VARP(sensitivity, 0, 10, 10000);
VARP(sensitivityscale, 1, 1, 10000);
VARP(invmouse, 0, 0, 1);

static void moden(int n) { client::addmsg(1, 2, SV_GAMEMODE, nextmode_ = n); }
COMMANDN(mode, moden, ARG_1INT);

static bool intermission = false;
static string clientmap;

const char *getclientmap(void) { return clientmap; }

// creation of scoreboard pseudo-menu
static bool scoreson = false;
static void showscores(bool on) {
  scoreson = on;
  menu::set(((int)on)-1);
}

void resetmovement(dynent *d) {
  d->k_left = false;
  d->k_right = false;
  d->k_up = false;
  d->k_down = false;
  d->jumpnext = false;
  d->strafe = 0;
  d->move = 0;
}

// reset player state not persistent accross spawns
static void spawnstate(dynent *d) {
  resetmovement(d);
  d->vel = zero;
  d->onfloor = false;
  d->timeinair = 0;
  d->health = 100;
  d->armour = 50;
  d->armourtype = A_BLUE;
  d->quadmillis = 0;
  d->lastattackgun = d->gunselect = GUN_SG;
  d->gunwait = 0;
  d->attacking = false;
  d->lastaction = 0;
  loopi(NUMGUNS) d->ammo[i] = 0;
  d->ammo[GUN_FIST] = 1;
  if (m_noitems) {
    d->gunselect = GUN_RIFLE;
    d->armour = 0;
    if (m_noitemsrail) {
      d->health = 1;
      d->ammo[GUN_RIFLE] = 100;
    } else {
      if (mode()==12) {
        d->gunselect = GUN_FIST;
        return;
      }  // eihrul's secret "instafist" mode
      d->health = 256;
      if (m_tarena) {
        int gun1 = rnd(4)+1;
        baseammo(d->gunselect = gun1);
        for (;;) {
          int gun2 = rnd(4)+1;
          if (gun1!=gun2) {
            baseammo(gun2);
            break;
          }
        }
      } else if (m_arena)    // insta arena
        d->ammo[GUN_RIFLE] = 100;
      else {
        loopi(4) baseammo(i+1);
        d->gunselect = GUN_CG;
      }
      d->ammo[GUN_CG] /= 2;
    }
  } else
    d->ammo[GUN_SG] = 5;
}

dynent *newdynent(void) {
  dynent *d = (dynent*) MALLOC(sizeof(dynent));
  d->o = zero;
  d->yaw = 270;
  d->pitch = 0;
  d->roll = 0;
  d->maxspeed = 22;
  d->outsidemap = false;
  d->inwater = false;
  d->radius = 1.1f;
  d->eyeheight = 3.2f;
  d->aboveeye = 0.7f;
  d->frags = 0;
  d->plag = 0;
  d->ping = 0;
  d->lastupdate = lastmillis();
  d->enemy = NULL;
  d->monsterstate = 0;
  d->name[0] = d->team[0] = 0;
  d->blocked = false;
  d->lifesequence = 0;
  d->state = CS_ALIVE;
  spawnstate(d);
  return d;
}

static void respawnself(void) {
  spawnplayer(player1);
  showscores(false);
}
COMMAND(showscores, ARG_DOWN);

void arenacount(dynent *d, int &alive, int &dead, char *&lastteam, bool &oneteam) {
  if (d->state!=CS_DEAD) {
    alive++;
    if (lastteam && strcmp(lastteam, d->team))
      oneteam = false;
    lastteam = d->team;
  } else
    dead++;
}

static int arenarespawnwait = 0;
static int arenadetectwait  = 0;

void arenarespawn(void) {
  if (arenarespawnwait) {
    if (arenarespawnwait<lastmillis()) {
      arenarespawnwait = 0;
      console::out("new round starting... fight!");
      respawnself();
    }
  } else if (arenadetectwait==0 || arenadetectwait<lastmillis()) {
    arenadetectwait = 0;
    int alive = 0, dead = 0;
    char *lastteam = NULL;
    bool oneteam = true;
    loopv(players)
      if (players[i])
        arenacount(players[i], alive, dead, lastteam, oneteam);
    arenacount(player1, alive, dead, lastteam, oneteam);
    if (dead>0 && (alive<=1 || (m_teammode && oneteam))) {
      console::out("arena round is over! next round in 5 seconds...");
      if (alive)
        console::out("team %s is last man standing", lastteam);
      else
        console::out("everyone died!");
      arenarespawnwait = lastmillis()+5000;
      arenadetectwait  = lastmillis()+10000;
      player1->roll = 0.f ;
    }
  }
}

void zapdynent(dynent *&d) {
  if (d) FREE(d);
  d = NULL;
}

static void otherplayers(void) {
  loopv(players) if (players[i]) {
    const int lagtime = lastmillis()-players[i]->lastupdate;
    if (lagtime>1000 && players[i]->state==CS_ALIVE) {
      players[i]->state = CS_LAGGED;
      continue;
    }
    // use physics to extrapolate player position
    if (lagtime && players[i]->state != CS_DEAD &&
      (!demo::playing() || i!=demo::clientnum()))
      physics::moveplayer(players[i], 2, false);
  }
}

static void respawn(void) {
  if (player1->state==CS_DEAD) {
    player1->attacking = false;
    if (m_arena) {
      console::out("waiting for new round to start...");
      return;
    }
    if (m_sp) {
      setnextmode(gamemode);
      client::changemap(clientmap);
      return;
    } // if we die in SP we try the same map again
    respawnself();
  }
}

static int sleepwait = 0;
static string sleepcmd;
static void sleepf(char *msec, char *cmd) {
  sleepwait = atoi(msec)+lastmillis();
  strcpy_s(sleepcmd, cmd);
}
COMMANDN(sleep, sleepf, ARG_2STR);

void updateworld(int millis) {
  if (lastmillis()) {
    setcurtime(millis - lastmillis());
    if (sleepwait && lastmillis()>sleepwait) {
      sleepwait = 0;
      cmd::execute(sleepcmd);
    }
    physics::physicsframe();
    checkquad(curtime());
    if (m_arena)
      arenarespawn();
    moveprojectiles(float(curtime()));
    demo::playbackstep();
    if (!demo::playing()) {
      if (client::getclientnum()>=0)
        shoot(player1, worldpos_); // only shoot when connected to server
      // do this first, so we have most accurate information when our player
      // moves
      client::gets2c();
    }
    otherplayers();
    if (!demo::playing()) {
      monsterthink();
      if (player1->state==CS_DEAD) {
        if (lastmillis()-player1->lastaction<2000) {
          player1->move = player1->strafe = 0;
          physics::moveplayer(player1, 10, false);
        } else if (!m_arena && !m_sp && lastmillis()-player1->lastaction>10000)
          respawn();
      } else if (!intermission) {
        physics::moveplayer(player1, 20, true);
        checkitems();
      }
      // do this last, to reduce the effective frame lag
      client::c2sinfo(player1);
    }
  }
  setlastmillis(millis);
}

void entinmap(dynent *d) {
  loopi(100) { // try max 100 times
    float dx = (rnd(21)-10)/10.0f*i;  // increasing distance
    float dy = (rnd(21)-10)/10.0f*i;
    d->o.x += dx;
    d->o.y += dy;
    if (physics::collide(d, true)) return;
    d->o.x -= dx;
    d->o.y -= dy;
  }
  console::out("can't find entity spawn spot! (%d, %d)",
               (int)d->o.x, (int)d->o.y);
}

static int spawncycle = -1;
static int fixspawn = 2;

void spawnplayer(dynent *d) {
  const int r = fixspawn-->0 ? 4 : rnd(10)+1;
  loopi(r) spawncycle = world::findentity(PLAYERSTART, spawncycle+1);
  if (spawncycle!=-1) {
    d->o.x = ents[spawncycle].x;
    d->o.y = ents[spawncycle].y;
    d->o.z = 10.f;// TODO handle real height later
    d->yaw = ents[spawncycle].attr1;
    d->pitch = 0;
    d->roll = 0;
  } else {
    d->o.x = d->o.y = 0.f;
    d->o.z = 4;
  }
  entinmap(d);
  spawnstate(d);
  d->state = CS_ALIVE;
}

#define DIRECTION(name,v,d,s,os) \
void name(bool isdown) { \
  player1->s = isdown; \
  player1->v = isdown ? d : (player1->os ? -(d) : 0); \
  player1->lastmove = lastmillis(); \
}\
COMMAND(name, ARG_DOWN);
DIRECTION(backward, move,   -1, k_down,  k_up);
DIRECTION(forward,  move,    1, k_up,    k_down);
DIRECTION(left,     strafe,  1, k_left,  k_right);
DIRECTION(right,    strafe, -1, k_right, k_left);
#undef DIRECTION

static void attack(bool on) {
  if (intermission)
    return;
  if (edit::mode())
    edit::editdrag(on);
  else if ((player1->attacking = on) != 0)
    respawn();
}
COMMAND(attack, ARG_DOWN);

static void jumpn(bool on) {
  if (!intermission && (player1->jumpnext = on)) respawn();
}
COMMANDN(jump, jumpn, ARG_DOWN);

void fixplayer1range(void) {
  const float MAXPITCH = 90.0f;
  if (player1->pitch>MAXPITCH) player1->pitch = MAXPITCH;
  if (player1->pitch<-MAXPITCH) player1->pitch = -MAXPITCH;
  while (player1->yaw<0.0f) player1->yaw += 360.0f;
  while (player1->yaw>=360.0f) player1->yaw -= 360.0f;
}

void mousemove(int dx, int dy) {
  const float SENSF = 33.0f; // try match quake sens
  if (player1->state==CS_DEAD || intermission) return;
  player1->yaw += (dx/SENSF)*(sensitivity/(float)sensitivityscale);
  player1->pitch -= (dy/SENSF)*(sensitivity/(float)sensitivityscale)*(invmouse ? -1 : 1);
  fixplayer1range();
}

void selfdamage(int damage, int actor, dynent *act) {
  if (player1->state!=CS_ALIVE || edit::mode() || intermission)
    return;
  rr::damageblend(damage);
  demo::blend(damage);
  // let armour absorb when possible
  int ad = damage*(player1->armourtype+1)*20/100;
  if (ad>player1->armour)
    ad = player1->armour;
  player1->armour -= ad;
  damage -= ad;
  float droll = damage/0.5f;
  // give player a kick depending on amount of damage
  player1->roll += player1->roll>0 ? droll :
    (player1->roll<0 ? -droll : (rnd(2) ? droll : -droll));
  if ((player1->health -= damage)<=0) {
    if (actor==-2)
      console::out("you got killed by %s!", act->name);
    else if (actor==-1) {
      actor = client::getclientnum();
      console::out("you suicided!");
      client::addmsg(1, 2, SV_FRAGS, --player1->frags);
    } else {
      dynent *a = getclient(actor);
      if (a) {
        if (isteam(a->team, player1->team))
          console::out("you got fragged by a teammate (%s)", a->name);
        else
          console::out("you got fragged by %s", a->name);
      }
    }
    showscores(true);
    client::addmsg(1, 2, SV_DIED, actor);
    player1->lifesequence++;
    player1->attacking = false;
    player1->state = CS_DEAD;
    player1->pitch = 0;
    player1->roll = 60;
    sound::play(sound::DIE1+rnd(2));
    spawnstate(player1);
    player1->lastaction = lastmillis();
  } else
    sound::play(sound::PAIN6);
}

void timeupdate(int timeremain) {
  if (!timeremain) {
    intermission = true;
    player1->attacking = false;
    console::out("intermission:");
    console::out("game has ended!");
    showscores(true);
  } else
    console::out("time remaining: %d minutes", timeremain);
}

dynent *getclient(int cn) {
  if (cn<0 || cn>=MAXCLIENTS) {
    client::neterr("clientnum");
    return NULL;
  }
  while (cn>=players.size())
    players.push_back(NULL);
  return players[cn] ? players[cn] : (players[cn] = newdynent());
}

void initclient(void) {
  clientmap[0] = 0;
  client::initclientnet();
}

void startmap(const char *name) {
  if (client::netmapstart() && m_sp) {
    gamemode = 0;
    console::out("coop sp not supported yet");
  }
  sleepwait = 0;
  monsterclear();
  projreset();
  spawncycle = -1;
  spawnplayer(player1);
  player1->frags = 0;
  loopv(players) if (players[i]) players[i]->frags = 0;
  resetspawns();
  strcpy_s(clientmap, name);
  if (edit::mode()) edit::toggleedit();
  cmd::setvar("gamespeed", 100);
  cmd::setvar("fog", 180);
  cmd::setvar("fogcolour", 0x8099B3);
  showscores(false);
  intermission = false;
  console::out("game mode is %s", modestr(mode()));
}

// render players & monsters: very messy ad-hoc handling of animation frames,
// should be made more configurable
static const int frame[] = {178, 184, 190, 137, 183, 189, 197, 164, 46, 51, 54, 32, 0,  0, 40, 1,  162, 162, 67, 168};
static const int range[] = {6,   6,   8,   28,  1,   1,   1,   1,   8,  19, 4,  18, 40, 1, 6,  15, 1,   1,   1,  1  };

void renderclient(dynent *d, bool team, const char *mdlname, bool hellpig, float scale) {
  int n = 3;
  float speed = 100.0f;
  float mz = d->o.z-d->eyeheight+1.55f*scale;
  int cast = (int) (uintptr_t) d;
  int basetime = -(((int)cast)&0xFFF);
  if (d->state==CS_DEAD) {
    int r;
    if (hellpig) {
      n = 2;
      r = range[3];
    } else {
      n = (int)cast%3;
      r = range[n];
    }
    basetime = d->lastaction;
    int t = lastmillis()-d->lastaction;
    if (t<0 || t>20000) return;
    if (t>(r-1)*100) {
      n += 4; if (t>(r+10)*100) {
        t -= (r+10)*100;
        mz -= t*t/10000000000.0f*t;
      }
    }
    if (mz<-1000) return;
  }
  else if (d->state==CS_EDITING) n = 16;
  else if (d->state==CS_LAGGED) n = 17;
  else if (d->monsterstate==M_ATTACKING) n = 8;
  else if (d->monsterstate==M_PAIN) n = 10;
  else if ((!d->move && !d->strafe) || !d->moving) n = 12;
  else if (!d->onfloor && d->timeinair>100) n = 18;
  else {
    n = 14;
    speed = 1200/d->maxspeed*scale;
    if (hellpig) speed = 300/d->maxspeed;
  }
  if (hellpig) {
    n++;
    scale *= 32;
    mz -= 1.9f;
  }
  rr::rendermodel(mdlname, frame[n], range[n], 0, 1.5f, vec3f(d->o.x, mz, d->o.y),
    d->yaw+90.f, d->pitch/2, team, scale, speed, 0, basetime);
}

void renderclients(void) {
  dynent *d;
  loopv(players)
    if ((d = players[i]) && (!demo::playing() || i!=demo::clientnum()))
      renderclient(d, isteam(player1->team, d->team), "monster/ogro", false, 1.f);
}

struct sline { string s; };
static vector<sline> scorelines;

static void renderscore(dynent *d) {
  sprintf_sd(lag)("%d", d->plag);
  sprintf_sd(name) ("(%s)", d->name);
  sprintf_s(scorelines.add().s)("%d\t%s\t%d\t%s\t%s",
            d->frags, d->state==CS_LAGGED ? "LAG" : lag,
            d->ping, d->team, d->state==CS_DEAD ? name : d->name);
  menu::manual(0, scorelines.size()-1, scorelines.back().s);
}

static const int maxteams = 4;
static char *teamname[maxteams];
static int teamscore[maxteams], teamsused;
static string teamscores;

static void addteamscore(dynent *d) {
  if (!d) return;
  loopi(teamsused) if (strcmp(teamname[i], d->team)==0) {
    teamscore[i] += d->frags;
    return;
  }
  if (teamsused==maxteams) return;
  teamname[teamsused] = d->team;
  teamscore[teamsused++] = d->frags;
}

void renderscores(void) {
  if (!scoreson) return;
  scorelines.resize(0);
  if (!demo::playing()) renderscore(player1);
  loopv(players) if (players[i])
    renderscore(players[i]);
  menu::sort(0, scorelines.size());
  if (m_teammode) {
    teamsused = 0;
    loopv(players)
      addteamscore(players[i]);
    if (!demo::playing()) addteamscore(player1);
    teamscores[0] = 0;
    loopj(teamsused) {
      sprintf_sd(sc)("[ %s: %d ]", teamname[j], teamscore[j]);
      strcat_s(teamscores, sc);
    }
    menu::manual(0, scorelines.size(), (char*) "");
    menu::manual(0, scorelines.size()+1, teamscores);
  }
}

void clean(void) {
  cleanentities();
  cleanmonsters();
}

} // namespace game
} // namespace cube
#endif // !defined(STANDALONE)

namespace cube {
namespace game {
static const char *modenames[] = {
  "SP", "DMSP", "ffa/default", "coopedit", "ffa/duel", "teamplay",
  "instagib", "instagib team", "efficiency", "efficiency team",
  "insta arena", "insta clan arena", "tactics arena", "tactics clan arena",
};
const char *modestr(int n) { return (n>=-2 && n<12) ? modenames[n+2] : "unknown"; }
} // namespace game
} // namespace cube

