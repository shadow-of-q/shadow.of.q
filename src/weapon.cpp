#include "cube.hpp"

namespace cube {
namespace game {

struct guninfo { short sound, attackdelay, damage, projspeed, part, kickamount; const char *name; };

static const int MONSTERDAMAGEFACTOR = 4;
static const int SGRAYS = 20;
static const float SGSPREAD = 2;
static const float RL_RADIUS = 5.f;
static const float RL_DAMRAD = 7.f; // hack
static vec3f sg[SGRAYS];

static const guninfo guns[NUMGUNS] = {
  {sound::PUNCH1,    250,  50, 0,   0,  1, "fist"          },
  {sound::SG,       1400,  10, 0,   0, 20, "shotgun"       },  // *SGRAYS
  {sound::CG,        100,  30, 0,   0,  7, "chaingun"      },
  {sound::RLFIRE,    800, 120, 80,  0, 10, "rocketlauncher"},
  {sound::RIFLE,    1500, 100, 0,   0, 30, "rifle"         },
  {sound::FLAUNCH,   200,  20, 50,  4,  1, "fireball"      },
  {sound::ICEBALL,   200,  40, 30,  6,  1, "iceball"       },
  {sound::SLIMEBALL, 200,  30, 160, 7,  1, "slimeball"     },
  {sound::PIGR1,     250,  50, 0,   0,  1, "bite"          }
};

void selectgun(int a, int b, int c)
{
  if (a<-1 || b<-1 || c<-1 || a>=NUMGUNS || b>=NUMGUNS || c>=NUMGUNS) return;
  int s = player1->gunselect;
  if (a>=0 && s!=a && player1->ammo[a]) s = a;
  else if (b>=0 && s!=b && player1->ammo[b]) s = b;
  else if (c>=0 && s!=c && player1->ammo[c]) s = c;
  else if (s!=GUN_RL && player1->ammo[GUN_RL]) s = GUN_RL;
  else if (s!=GUN_CG && player1->ammo[GUN_CG]) s = GUN_CG;
  else if (s!=GUN_SG && player1->ammo[GUN_SG]) s = GUN_SG;
  else if (s!=GUN_RIFLE && player1->ammo[GUN_RIFLE]) s = GUN_RIFLE;
  else s = GUN_FIST;
  if (s!=player1->gunselect) sound::playc(sound::WEAPLOAD);
  player1->gunselect = s;
  //console::out("%s selected", (int)guns[s].name);
}

int reloadtime(int gun) { return guns[gun].attackdelay; }

void weapon(char *a1, char *a2, char *a3) {
  selectgun(a1[0]?atoi(a1):-1, a2[0]?atoi(a2):-1, a3[0]?atoi(a3):-1);
}

COMMAND(weapon, ARG_3STR);

void createrays(vec3f &from, vec3f &to)             // create random spread of rays for the shotgun
{
  const float dist = distance(from, to);
  const float f = dist*SGSPREAD/1000;
#define RNDD (rnd(101)-50)*f
  loopi(SGRAYS) {
    const vec3f r(RNDD, RNDD, RNDD);
    sg[i] = to;
    sg[i] += r;
  }
#undef RNDD
}

bool intersect(dynent *d, vec3f &from, const vec3f &to)   // if lineseg hits entity bounding box
{
  vec3f v = to, w = d->o;
  const vec3f *p;
  v -= from;
  w -= from;
  float c1 = dot(w, v);

  if (c1<=0)
    p = &from;
  else {
    float c2 = dot(v, v);
    if (c2<=c1) p = &to;
    else {
      float f = c1/c2; v *= f;
      v += from;
      p = &v;
    }
  }

  return p->x <= d->o.x+d->radius
      && p->x >= d->o.x-d->radius
      && p->y <= d->o.y+d->radius
      && p->y >= d->o.y-d->radius
      && p->z <= d->o.z+d->aboveeye
      && p->z >= d->o.z-d->eyeheight;
}

char *playerincrosshair(void)
{
  if (demo::playing()) return NULL;
  loopv(players) {
    dynent *o = players[i];
    if (!o) continue;
    if (intersect(o, player1->o, game::worldpos())) return o->name;
  }
  return NULL;
}

static const int MAXPROJ = 100;
struct projectile { vec3f o, to; float speed; dynent *owner; int gun; bool inuse, local; };
static projectile projs[MAXPROJ];

void projreset(void) { loopi(MAXPROJ) projs[i].inuse = false; }

static void newprojectile(const vec3f &from, const vec3f &to, float speed, bool local, dynent *owner, int gun) {
  loopi(MAXPROJ) {
    projectile *p = &projs[i];
    if (p->inuse) continue;
    p->inuse = true;
    p->o = from;
    p->to = to;
    p->speed = speed;
    p->local = local;
    p->owner = owner;
    p->gun = gun;
    return;
  }
}

static void hit(int target, int damage, dynent *d, dynent *at) {
  if (d==player1)
    game::selfdamage(damage, at==player1 ? -1 : -2, at);
  else if (d->monsterstate)
    monsterpain(d, damage, at);
  else {
    client::addmsg(1, 4, SV_DAMAGE, target, damage, d->lifesequence);
    sound::play(sound::PAIN1+rnd(5), &d->o);
  }
  rr::particle_splash(3, damage, 1000, d->o);
  demo::damage(damage, d->o);
}

static void radialeffect(dynent *o, const vec3f &v, int cn, int qdam, dynent *at) {
  if (o->state!=CS_ALIVE) return;
  vec3f temp = o->o-v;
  float dist = length(temp);
  dist -= 2; // account for eye distance imprecision
  if (dist<RL_DAMRAD) {
    if (dist<0) dist = 0;
    const int damage = (int)(qdam*(1-(dist/RL_DAMRAD)));
    hit(cn, damage, o, at);
    temp *= (RL_DAMRAD-dist)*damage/800.f;
    o->vel += temp;
  }
}

static void splash(projectile *p, const vec3f &v, const vec3f &vold, int notthisplayer, int notthismonster, int qdam) {
  rr::particle_splash(0, 50, 300, v);
  p->inuse = false;
  if (p->gun!=GUN_RL)
    sound::play(sound::FEXPLODE, &v);
  else {
    sound::play(sound::RLHIT, &v);
    rr::newsphere(v, RL_RADIUS, 0);
    if (!p->local) return;
    radialeffect(player1, v, -1, qdam, p->owner);
    loopv(players) {
      if (i==notthisplayer) continue;
      dynent *o = players[i];
      if (!o) continue;
      radialeffect(o, v, i, qdam, p->owner);
    }
    dvector &mv = getmonsters();
    loopv(mv) if (i!=notthismonster) radialeffect(mv[i], v, i, qdam, p->owner);
  }
}

INLINE void projdamage(dynent *o, projectile *p, const vec3f &v, int i, int im, int qdam) {
  if (o->state!=CS_ALIVE) return;
  if (intersect(o, p->o, v)) {
    splash(p, v, p->o, i, im, qdam);
    hit(i, qdam, o, p->owner);
  }
}

void moveprojectiles(float time) {
  loopi(MAXPROJ) {
    projectile *p = &projs[i];
    if (!p->inuse) continue;
    int qdam = guns[p->gun].damage*(p->owner->quadmillis ? 4 : 1);
    if (p->owner->monsterstate) qdam /= MONSTERDAMAGEFACTOR;
    vec3f v = p->to-p->o;
    const float dist = length(v);
    float dtime = dist*1000/p->speed;
    if (time>dtime) dtime = time;
    v *= time/dtime;
    v += p->o;
    if (p->local) {
      loopv(players) {
        dynent *o = players[i];
        if (!o) continue;
        projdamage(o, p, v, i, -1, qdam);
      }
      if (p->owner!=player1) projdamage(player1, p, v, -1, -1, qdam);
      dvector &mv = getmonsters();
      loopv(mv) if (!rejectxy(mv[i]->o, v, 10.0f) && mv[i]!=p->owner)
        projdamage(mv[i], p, v, -1, i, qdam);
    }
    if (p->inuse) {
      if (time==dtime)
        splash(p, v, p->o, -1, -1, qdam);
      else {
        if (p->gun==GUN_RL)
          rr::particle_splash(5, 2, 200, v);
        else {
          rr::particle_splash(1, 1, 200, v);
          rr::particle_splash(guns[p->gun].part, 1, 1, v);
        }
      }
    }
    p->o = v;
  }
}

// create visual effect from a shot
void shootv(int gun, const vec3f &from, const vec3f &to, dynent *d, bool local)
{
  sound::play(guns[gun].sound, d==player1 ? NULL : &d->o);
  int pspeed = 25;
  switch (gun) {
    case GUN_FIST:
      break;
    case GUN_SG:
      loopi(SGRAYS) rr::particle_splash(0, 5, 200, sg[i]);
      break;
    case GUN_CG:
      rr::particle_splash(0, 100, 250, to);
    break;
    case GUN_RL:
    case GUN_FIREBALL:
    case GUN_ICEBALL:
    case GUN_SLIMEBALL:
      pspeed = guns[gun].projspeed;
      if (d->monsterstate) pspeed /= 2;
      newprojectile(from, to, float(pspeed), local, d, gun);
    break;
    case GUN_RIFLE:
      rr::particle_splash(0, 50, 200, to);
      rr::particle_trail(1, 500, from, to);
    break;
  }
}

void hitpush(int target, int damage, dynent *d, dynent *at, vec3f &from, vec3f &to) {
  hit(target, damage, d, at);
  const vec3f v = to-from;
  const float dist = length(v);
  d->vel += damage/dist/50.f*v;
}

void raydamage(dynent *o, vec3f &from, vec3f &to, dynent *d, int i) {
  if (o->state!=CS_ALIVE) return;
  int qdam = guns[d->gunselect].damage;
  if (d->quadmillis) qdam *= 4;
  if (d->monsterstate) qdam /= MONSTERDAMAGEFACTOR;
  if (d->gunselect==GUN_SG) {
    int damage = 0;
    loop(r, SGRAYS) if (intersect(o, from, sg[r])) damage += qdam;
    if (damage) hitpush(i, damage, o, d, from, to);
  }
  else if (intersect(o, from, to)) hitpush(i, qdam, o, d, from, to);
}

void shoot(dynent *d, vec3f &targ) {
  int attacktime = game::lastmillis()-d->lastaction;
  if (attacktime<d->gunwait) return;
  d->gunwait = 0;
  if (!d->attacking) return;
  d->lastaction = game::lastmillis();
  d->lastattackgun = d->gunselect;
  if (!d->ammo[d->gunselect]) {
    sound::playc(sound::NOAMMO);
    d->gunwait = 250;
    d->lastattackgun = -1;
    return;
  }
  if (d->gunselect) d->ammo[d->gunselect]--;
  vec3f from = d->o;
  vec3f to = targ;
  from.z -= 0.2f; // below eye

  vec3f unitv = normalize(to-from);
  const vec3f kickback = -unitv*float(guns[d->gunselect].kickamount)*0.01f;
  d->vel += kickback;
  if (d->pitch<80.0f) d->pitch += guns[d->gunselect].kickamount*0.05f;


  if (d->gunselect==GUN_FIST || d->gunselect==GUN_BITE) {
    unitv *= 3.f; // punch range
    to = from;
    to += unitv;
  }
  if (d->gunselect==GUN_SG) createrays(from, to);

  if (d->quadmillis && attacktime>200) sound::playc(sound::ITEMPUP);
  shootv(d->gunselect, from, to, d, true);
  if (!d->monsterstate) {
    const vec3i ifrom=DMF*from, ito=DMF*to;
    client::addmsg(1,8,SV_SHOT,d->gunselect,ifrom.x,ifrom.y,ifrom.z,ito.x,ito.y,ito.z);
  }
  d->gunwait = guns[d->gunselect].attackdelay;

  if (guns[d->gunselect].projspeed) return;

  loopv(players) {
    dynent *o = players[i];
    if (!o) continue;
    raydamage(o, from, to, d, i);
  }

  dvector &v = getmonsters();
  loopv(v) if (v[i]!=d) raydamage(v[i], from, to, d, -2);

  if (d->monsterstate) raydamage(player1, from, to, d, -1);
}

} // namespace game
} // namespace cube

