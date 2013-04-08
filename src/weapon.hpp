#pragma once
#include "entities.hpp"
#include "tools.hpp"

namespace cube {
namespace game {

void selectgun(int a = -1, int b = -1, int c =-1);
void shoot(game::dynent *d, vec3f &to);
void shootv(int gun, vec3f &from, vec3f &to, game::dynent *d = 0, bool local = false);
void createrays(vec3f &from, vec3f &to);
void moveprojectiles(float time);
void projreset(void);
char *playerincrosshair(void);
int reloadtime(int gun);

} // namespace weapon
} // namespace cube

