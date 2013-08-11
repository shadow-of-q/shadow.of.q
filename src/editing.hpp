#pragma once
#include "world.hpp"
#include "base/math.hpp"

namespace cube {
namespace edit {

bool mode(void);
void cursorupdate(void);
void toggleedit(void);
void editdrag(bool isdown);
bool noteditmode(void);
void pruneundos(int maxremain = 0);
void setcube(const vec3i &xyz, const world::brickcube &c, bool undoable);

} // namespace edit
} // namespace cube

