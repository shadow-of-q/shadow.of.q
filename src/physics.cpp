// no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply
// tweaked until they "felt right", and have no basis in reality.  Collision
// detection is simplistic but very robust (uses discrete steps at fixed fps).
#include "cube.hpp"

namespace cube {
namespace physics {

  // collide with player or monster
  static bool plcollide(const aabb &box, const game::dynent *o) {
    if (o->state!=CS_ALIVE) return true;
    const auto oaabb = getaabb(o);
    return !intersect(box, oaabb);
  }

  // collide with a map model
  static bool mmcollide(const aabb &box) {
    loopv(game::ents) {
      game::entity &e = game::ents[i];
      if (e.type != game::MAPMODEL) continue;
      const game::mapmodelinfo &mmi = rr::getmminfo(e.attr2);
      if (!&mmi || !mmi.h) continue;
      const auto emin = vec3f(e.x,e.y,e.z)-vec3f(mmi.rad,mmi.rad,0.f);
      const auto emax = vec3f(e.x,e.y,e.z)+vec3f(mmi.rad,mmi.rad,mmi.h);
      const aabb eaabb(emin,emax);
      if (intersect(box, eaabb)) return false;
    }
    return true;
  }

  // get bounding volume of the given deformed cube
  INLINE aabb getaabb(vec3i xyz) {
    aabb box(FLT_MAX,-FLT_MAX);
    loopi(8) {
      const auto v = world::getpos(xyz+cubeiverts[i]);
      box.pmin = min(v, box.pmin);
      box.pmax = max(v, box.pmax);
    }
    return box;
  }

  // collide with the map
  static bool mapcollide(const aabb &box) {
    const auto pmin = vec3i(box.pmin)-vec3i(one);
    const auto pmax = vec3i(box.pmax)+vec3i(one);
    loopxyz(pmin, pmax,
      if (world::getcube(xyz).mat != world::EMPTY && intersect(getaabb(xyz), box))
        return false;);
    return true;
  }

  // all collision happens here. spawn is a dirty side effect used in
  // spawning. drop & rise are supplied by the physics below to indicate
  // gravity/push for current mini-timestep
  bool collide(game::dynent *d, bool spawn) {
    const auto box = getaabb(d);

    // collide with map
    if (!mapcollide(box)) return false;

    // collide with other players
    loopv(game::players) {
      game::dynent *o = game::players[i];
      if (!o || o==d) continue;
      if (!plcollide(box, o)) return false;
    }
    if (d!=game::player1)
      if (!plcollide(box, game::player1)) return false;
    auto &v = game::getmonsters();
    loopv(v) if (!plcollide(box, v[i])) return false;

    // collide with map models
    if (!mmcollide(box)) return false;
    return true;
  }

  VARP(maxroll, 0, 3, 20);

  static int physicsfraction = 0, physicsrepeat = 0;
  static const int MINFRAMETIME = 20; // physics simulated at 50fps or better

  void physicsframe(void) { // optimally schedule physics frames inside the graphics frames
    if (game::curtime()>=MINFRAMETIME) {
      int faketime = game::curtime()+physicsfraction;
      physicsrepeat = faketime/MINFRAMETIME;
      physicsfraction = faketime-physicsrepeat*MINFRAMETIME;
    } else
      physicsrepeat = 1;
  }

  // main physics routine, moves a player/monster for a curtime step.
  // moveres indicated the physics precision (which is lower for monsters and
  // client::multiplayer prediction).
  // local is false for client::multiplayer prediction
  void moveplayer(game::dynent *pl, int moveres, bool local, int curtime) {
    const bool water = false;//world::waterlevel() > pl->o.z-0.5f;
    const bool floating = (edit::mode() && local) || pl->state==CS_EDITING;

    vec3f d; // vector of direction we ideally want to move in
    d.x = float(pl->move*cos(deg2rad(pl->yaw-90.f)));
    d.y = float(pl->move*sin(deg2rad(pl->yaw-90.f)));
    d.z = 0.f;

    if (floating || water) {
      d.x *= float(cos(deg2rad(pl->pitch)));
      d.y *= float(cos(deg2rad(pl->pitch)));
      d.z  = float(pl->move*sin(deg2rad(pl->pitch)));
    }

    d.x += float(pl->strafe*cos(deg2rad(pl->yaw-180.f)));
    d.y += float(pl->strafe*sin(deg2rad(pl->yaw-180.f)));

    const float speed = curtime/(water? 2000.0f : 1000.0f)*pl->maxspeed;
    const float friction = water ? 20.0f : (pl->onfloor||floating ? 6.0f : 30.0f);
    const float fpsfric = friction/curtime*20.0f;

    // slowly apply friction and direction to velocity, gives a smooth movement
    pl->vel *= fpsfric-1.f;
    pl->vel += d;
    pl->vel /= fpsfric;
    d = pl->vel;
    d *= speed; // d is now frametime based velocity vector

    pl->blocked = false;
    pl->moving = true;

    if (floating) { // just apply velocity
      pl->o += d;
      if (pl->jumpnext) {
        pl->jumpnext = false;
        pl->vel.z = 2.f;
      }
    } else { // apply velocity with collision
      if (pl->onfloor || water) {
        if (pl->jumpnext) {
          pl->jumpnext = false;
          pl->vel.z = 1.7f; // physics impulse upwards
          if (water) { // dampen velocity change even harder, gives correct water feel
            pl->vel.x /= 8.f;
            pl->vel.y /= 8.f;
          }
          if (local)
            sound::playc(sound::JUMP);
          else if (pl->monsterstate)
            sound::play(sound::JUMP, &pl->o);
        } else if (pl->timeinair>800) { // if we land after long time must have been a high jump, add sound
          if (local)
            sound::playc(sound::LAND);
          else if (pl->monsterstate)
            sound::play(sound::LAND, &pl->o);
        }
        pl->timeinair = 0;
      } else
        pl->timeinair += curtime;

      const float gravity = 20.f;
      const float f = 1.0f/float(moveres);
      float dropf = ((gravity-1.f)+pl->timeinair/15.0f); // incorrect, but works fine
      if (water) { // float slowly down in water
        dropf = 5.f;
        pl->timeinair = 0.f;
      }
      const float drop = dropf*curtime/gravity/100/moveres; // at high fps, gravity kicks in too fast

      loopi(moveres) { // discrete steps collision detection & sliding
        // try to apply gravity
        pl->o.z -= drop;
        if (!collide(pl, false)) {
          pl->o.z += drop;
          pl->onfloor = true;
        } else
          pl->onfloor = false;

        // try move forward
        pl->o += f*d;
        if (collide(pl, false)) continue;

        // player stuck, try slide along all axis
        bool successful = false;
        pl->blocked = true;
        loopi(3) {
          pl->o[i] -= f*d[i];
          if (collide(pl, false)) {
            successful = true;
            d[i] = 0.f;
            break;
          }
          pl->o[i] += f*d[i];
        }
        if (successful) continue;

        // try just dropping down
        pl->moving = false;
        pl->o.x -= f*d.x;
        pl->o.y -= f*d.y;
        if (collide(pl, false)) {
          d.x = d.y = 0.f;
          continue;
        }
        pl->o.z -= f*d.z;
        break;
      }
    }
    pl->outsidemap = false;

    // automatically apply smooth roll when strafing
    if (pl->strafe==0)
      pl->roll = pl->roll/(1.f+float(sqrt(float(curtime)))/25.f);
    else {
      pl->roll += pl->strafe*curtime/-30.0f;
      if (pl->roll>maxroll) pl->roll = float(maxroll);
      if (pl->roll<-maxroll) pl->roll = -float(maxroll);
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

