#pragma once
#include "entities.hpp"
#include "tools.hpp"

namespace cube {
namespace world {
struct block { int x, y, xs, ys; };

enum {
  MAPVERSION = 5, // bump if map format changes, see worldio.cpp
  SMALLEST_FACTOR = 6, // determines number of mips there can be
  DEFAULT_FACTOR = 8,
  LARGEST_FACTOR = 11 // 10 is already insane
};

void setup(int factor);
// used for edit mode ent display
int closestent(void);
int findentity(int type, int index = 0);
// trigger tag
void trigger(int tag, int type, bool savegame);
// reset for editing or map saving
void resettagareas(void);
// set for playing
void settagareas(void);
// create a new entity
game::entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4);
// save the world as .cgz file
void save(const char *fname);
// load the world from .cgz file
void load(const char *mname);
void writemap(const char *mname, int msize, uchar *mdata);
uchar *readmap(const char *mname, int *msize);
// test occlusion for a cube (v = viewer, c = cube to test)
int isoccluded(float vx, float vy, float cx, float cy, float csize);
// return the water level for the loaded map
int waterlevel(void);
// return the name of the current map
char *maptitle(void);

} // namespace world
} // namespace cube

