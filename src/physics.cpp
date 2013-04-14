// no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply
// tweaked until they "felt right", and have no basis in reality.  Collision
// detection is simplistic but very robust (uses discrete steps at fixed fps).
#include "cube.hpp"

namespace cube {
namespace physics {

  // collide with player or monster
  bool plcollide(game::dynent *d, game::dynent *o, float &headspace, float &hi, float &lo) {
    if (o->state!=CS_ALIVE) return true;
    const float r = o->radius+d->radius;
    if (fabs(o->o.x-d->o.x)<r && fabs(o->o.y-d->o.y)<r) {
      if (d->o.z-d->eyeheight<o->o.z-o->eyeheight) {
        if (o->o.z-o->eyeheight<hi)
          hi = o->o.z-o->eyeheight-1;
      } else if (o->o.z+o->aboveeye>lo)
        lo = o->o.z+o->aboveeye+1;
      if (fabs(o->o.z-d->o.z)<o->aboveeye+d->eyeheight) return false;
      if (d->monsterstate) return false; // hack
      headspace = d->o.z-o->o.z-o->aboveeye-d->eyeheight;
      if (headspace<0) headspace = 10;
    }
    return true;
  }

  // collide with a map model
  static void mmcollide(game::dynent *d, float &hi, float &lo) {
    loopv(game::ents) {
      game::entity &e = game::ents[i];
      if (e.type != game::MAPMODEL) continue;
      game::mapmodelinfo &mmi = rr::getmminfo(e.attr2);
      if (!&mmi || !mmi.h) continue;
      const float r = mmi.rad+d->radius;
      if (fabs(e.x-d->o.x)<r && fabs(e.y-d->o.y)<r) {
        const float mmz = mmi.zoff+e.attr3;
        if (d->o.z-d->eyeheight<mmz) {
          if (mmz<hi) hi = mmz;
        } else if (mmz+mmi.h>lo)
          lo = mmz+mmi.h;
      }
    }
  }

  // all collision happens here. spawn is a dirty side effect used in
  // spawning. drop & rise are supplied by the physics below to indicate
  // gravity/push for current mini-timestep
  bool collide(game::dynent *d, bool spawn, float drop, float rise) {
    float hi = 127, lo = 0;
    if (hi-lo < d->eyeheight+d->aboveeye) return false;

    float headspace = 10;
    loopv(game::players) { // collide with other players
      game::dynent *o = game::players[i];
      if (!o || o==d) continue;
      if (!plcollide(d, o, headspace, hi, lo)) return false;
    }
    if (d!=game::player1)
      if (!plcollide(d, game::player1, headspace, hi, lo))
        return false;
    auto &v = game::getmonsters();
    // this loop can be a performance bottleneck with many monster on a slow
    // cpu, should replace with a blockmap but seems mostly fast enough
    loopv(v)
      if (!rejectxy(d->o, v[i]->o, 7.0f) && d!=v[i] && !plcollide(d, v[i], headspace, hi, lo))
        return false;
    headspace -= 0.01f;

    mmcollide(d, hi, lo); // collide with map models

    if (spawn) {
      d->o.z = lo+d->eyeheight; // just drop to floor (sideeffect)
      d->onfloor = true;
    } else {
      const float space = d->o.z-d->eyeheight-lo;
      if (space<0) {
        if (space>-0.01) d->o.z = lo+d->eyeheight; // stick on step
        else if (space>-1.26f) d->o.z += rise; // rise thru stair
        else return false;
      } else
        d->o.z -= min(min(drop, space), headspace); // gravity

      const float space2 = hi-(d->o.z+d->aboveeye);
      if (space2<0) {
        if (space2<-0.1) return false; // hack alert!
        d->o.z = hi-d->aboveeye; // glue to ceiling
        d->vel.z = 0; // cancel out jumping velocity
      }
      d->onfloor = d->o.z-d->eyeheight-lo<0.001f;
    }
    return true;
  }

  static float rad(float x) { return x*3.14159f/180; };

  VARP(maxroll, 0, 3, 20);

  static int physicsfraction = 0, physicsrepeat = 0;
  static const int MINFRAMETIME = 20; // physics always simulated at 50fps or better

  void physicsframe(void) { // optimally schedule physics frames inside the graphics frames
    if (game::curtime()>=MINFRAMETIME) {
      int faketime = game::curtime()+physicsfraction;
      physicsrepeat = faketime/MINFRAMETIME;
      physicsfraction = faketime-physicsrepeat*MINFRAMETIME;
    } else
      physicsrepeat = 1;
  }

  // main physics routine, moves a player/monster for a curtime() step.  moveres
  // indicated the physics precision (which is lower for monsters and
  // client::multiplayer prediction). local is false for client::multiplayer
  // prediction
  void moveplayer(game::dynent *pl, int moveres, bool local, int curtime) {
    const bool water = world::waterlevel()>pl->o.z-0.5f;
    const bool floating = (edit::mode() && local) || pl->state==CS_EDITING;

    vec3f d; // vector of direction we ideally want to move in
    d.x = (float)(pl->move*cos(rad(pl->yaw-90)));
    d.y = (float)(pl->move*sin(rad(pl->yaw-90)));
    d.z = 0;

    if (floating || water) {
      d.x *= (float)cos(rad(pl->pitch));
      d.y *= (float)cos(rad(pl->pitch));
      d.z = (float)(pl->move*sin(rad(pl->pitch)));
    }

    d.x += (float)(pl->strafe*cos(rad(pl->yaw-180)));
    d.y += (float)(pl->strafe*sin(rad(pl->yaw-180)));

    const float speed = curtime/(water ? 2000.0f : 1000.0f)*pl->maxspeed;
    const float friction = water ? 20.0f : (pl->onfloor || floating ? 6.0f : 30.0f);
    const float fpsfric = friction/curtime*20.0f;

    pl->vel *= fpsfric-1; // slowly apply friction and direction to velocity, gives a smooth movement
    pl->vel += d;
    pl->vel /= fpsfric;
    d = pl->vel;
    d *= speed; // d is now frametime based velocity vector

    pl->blocked = false;
    pl->moving = true;

    if (floating) { // just apply velocity
      pl->o += d;
      if (pl->jumpnext) { pl->jumpnext = false; pl->vel.z = 2;    }
    } else { // apply velocity with collision
      if (pl->onfloor || water) {
        if (pl->jumpnext) {
          pl->jumpnext = false;
          pl->vel.z = 1.7f; // physics impulse upwards
          if (water) { pl->vel.x /= 8; pl->vel.y /= 8; }; // dampen velocity change even harder, gives correct water feel
          if (local) sound::playc(sound::JUMP);
          else if (pl->monsterstate) sound::play(sound::JUMP, &pl->o);
        } else if (pl->timeinair>800)  // if we land after long time must have been a high jump, make thud sound
        {
          if (local) sound::playc(sound::LAND);
          else if (pl->monsterstate) sound::play(sound::LAND, &pl->o);
        }
        pl->timeinair = 0;
      } else
        pl->timeinair += curtime;

      const float gravity = 20;
      const float f = 1.0f/moveres;
      float dropf = ((gravity-1)+pl->timeinair/15.0f);        // incorrect, but works fine
      if (water) { dropf = 5; pl->timeinair = 0; };            // float slowly down in water
      const float drop = dropf*curtime/gravity/100/moveres;   // at high fps, gravity kicks in too fast
      const float rise = speed/moveres/1.2f;                  // extra smoothness when lifting up stairs

      loopi(moveres) { // discrete steps collision detection & sliding
        // try move forward
        pl->o.x += f*d.x;
        pl->o.y += f*d.y;
        pl->o.z += f*d.z;
        if (collide(pl, false, drop, rise)) continue;
        // player stuck, try slide along y axis
        pl->blocked = true;
        pl->o.x -= f*d.x;
        if (collide(pl, false, drop, rise)) { d.x = 0; continue; };
        pl->o.x += f*d.x;
        // still stuck, try x axis
        pl->o.y -= f*d.y;
        if (collide(pl, false, drop, rise)) { d.y = 0; continue; };
        pl->o.y += f*d.y;
        // try just dropping down
        pl->moving = false;
        pl->o.x -= f*d.x;
        pl->o.y -= f*d.y;
        if (collide(pl, false, drop, rise)) { d.y = d.x = 0; continue; };
        pl->o.z -= f*d.z;
        break;
      }
    }

    pl->outsidemap = false;

    // automatically apply smooth roll when strafing
    if (pl->strafe==0)
      pl->roll = pl->roll/(1+(float)sqrt((float)curtime)/25);
    else {
      pl->roll += pl->strafe*curtime/-30.0f;
      if (pl->roll>maxroll) pl->roll = (float)maxroll;
      if (pl->roll<-maxroll) pl->roll = (float)-maxroll;
    }

    // play sounds on water transitions
    if (!pl->inwater && water) {
      sound::play(sound::SPLASH2, &pl->o);
      pl->vel.z = 0.f;
    } else if (pl->inwater && !water)
      sound::play(sound::SPLASH1, &pl->o);
    pl->inwater = water;
  }

  void moveplayer(game::dynent *pl, int moveres, bool local) {
    loopi(physicsrepeat)
      moveplayer(pl, moveres, local,
        i ? game::curtime()/physicsrepeat :
            game::curtime()-game::curtime()/physicsrepeat*(physicsrepeat-1));
  }

} // namespace physics
} // namespace cube

