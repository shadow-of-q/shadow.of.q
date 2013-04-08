// most map editing commands go here, entity editing commands are
// in world.cpp
#include "cube.h"

namespace cube {

// XXX
bool editmode = false;

namespace edit {

VAR(editing,0,0,1);

void toggleedit(void)
{
  if (game::player1->state==CS_DEAD) return; // do not allow dead players to edit to avoid state confusion
  if (!editmode && !client::allowedittoggle()) return; // not in most client::multiplayer modes
  if (!(editmode = !editmode))
    game::entinmap(game::player1); // find spawn closest to current floating pos
  else {
    game::player1->health = 100;
    if (m_classicsp) game::monsterclear(); // all monsters back at their spawns for editing
    game::projreset();
  }
  keyrepeat(editmode);
  // selset = false;
  editing = editmode;
}
COMMANDN(edittoggle, toggleedit, ARG_NONE);

bool noteditmode(void) {
  if (!editmode)
    console::out("this function is only allowed in edit mode");
  return !editmode;
}

void pruneundos(int maxremain) {}

} // namespace edit
} // namespace cube

