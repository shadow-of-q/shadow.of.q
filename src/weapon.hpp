#pragma once
#include "entities.hpp"
#include "base/tools.hpp"

namespace cube {
namespace game {

void selectgun(int a = -1, int b = -1, int c =-1);
void shoot(game::dynent *d, vec3f &to);
void shootv(int gun, const vec3f &from, const vec3f &to, dynent *d = NULL, bool local = false);
void createrays(const vec3f &from, const vec3f &to);
void moveprojectiles(float time);
void projreset(void);
char *playerincrosshair(void);
int reloadtime(int gun);

} // namespace weapon
} // namespace cube

