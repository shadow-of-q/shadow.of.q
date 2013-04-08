#pragma once
#include "entities.hpp"

namespace cube {
namespace physics {

void moveplayer(game::dynent *pl, int moveres, bool local);
bool collide(game::dynent *d, bool spawn, float drop, float rise);
void setentphysics(int mml, int mmr);
void physicsframe(void);

} // namespace physics
} // namespace cube

