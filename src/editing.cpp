#include "cube.hpp"

namespace cube {
namespace edit {

VAR(editing,0,0,1);
static world::block _sel = {
  cmd::variable("selx",  0, 0, 4096, &_sel.x,  NULL, false),
  cmd::variable("sely",  0, 0, 4096, &_sel.y,  NULL, false),
  cmd::variable("selxs", 0, 0, 4096, &_sel.xs, NULL, false),
  cmd::variable("selys", 0, 0, 4096, &_sel.ys, NULL, false),
};

static bool editmode = false;
bool mode(void) { return editmode; }

void toggleedit(void) {
  if (game::player1->state==CS_DEAD) return; // do not allow dead players to edit to avoid state confusion
  if (!editmode && !client::allowedittoggle()) return; // not in most client::multiplayer modes
#if 0
  if (!(editmode = !editmode))
    game::entinmap(game::player1); // find spawn closest to current floating pos
  else {
#else
  if ((editmode = !editmode)) {
#endif
    game::player1->health = 100;
    if (m_classicsp) game::monsterclear(); // all monsters back at their spawns for editing
    game::projreset();
  }
  keyrepeat(editmode);
  // selset = false;
  editing = editmode;
}
COMMANDN(edittoggle, toggleedit, ARG_NONE);

void pruneundos(int maxremain) {}

// two mode of editions: extrusion of cubes / displacement of corners
VARF(editcorner, 0, 0, 1, console::out("edit mode is 'edit%s", editcorner?"corner'":"cube'"));

static bool anyundercursor = false; // do we see anything on the center of screen?
static vec3i undercursorcube(zero); // cube under the cursor
static vec3i undercursorcorner(zero); // corner under the cursor
static int undercursorface = 0; // which face we specifically select (0 to 5)

static bool anyselected = false; // curr selection (face, start and end cubes)
static vec3i cubestart(zero), cubeend(zero); // start / end of cube dragging
static vec3i cornerstart(zero), cornerend(zero); // start / end of corner dragging
static int selectedface = 0, disttoselected = 0;

static u32 symmetrydir = 0; // round robin symmetries

bool noteditmode(void) {
  if (!editmode)
    console::out("this function is only allowed in edit mode");
  return !editmode;
}

static bool noselection(void) {
  if (!anyselected) console::out("no selection");
  return !anyselected;
}

#define EDIT    if (noteditmode()) return;
#define EDITSEL if (noteditmode() || noselection()) return;

static bool dragging = false;
void editdrag(bool isdown) {
  if (isdown) {
    dragging = anyselected = anyundercursor;
    cubestart = cubeend = undercursorcube;
    cornerstart = cornerend = undercursorcorner;
    selectedface = undercursorface;
    disttoselected = 0;
  }
  if (!isdown) {
    dragging = false;
    cubeend = undercursorcube;
    cornerend = undercursorcorner;
    symmetrydir = 0;
  }
}

/*-------------------------------------------------------------------------
 - undo / redo
 -------------------------------------------------------------------------*/
struct clone {
  INLINE clone(world::brickcube c, vec3i xyz, u32 action = 0, bool onlypos = false) :
    c(c), xyz(xyz), action(action), onlypos(onlypos) {}
  INLINE clone(const clone &other) :
    c(other.c), xyz(other.xyz), action(other.action), onlypos(other.onlypos) {}
  INLINE clone &operator= (const clone &other) {
    c = other.c;
    xyz = other.xyz;
    action = other.action;
    onlypos = other.onlypos;
    return *this;
  }
  world::brickcube c;
  vec3i xyz;
  s32 action;
  bool onlypos;
};

static vector<clone> undobuffer;
static int undocurr = 0, undoaction = 0;
static bool newundoaction = false;

static void newundobuffer(void) { newundoaction = true; }
static void saveundocube(const vec3i &xyz) {
  if (newundoaction) {
    newundoaction=false;
    undoaction++;
    undobuffer.setsize(undocurr);
  }
  const auto copy = clone(world::getcube(xyz), xyz, undoaction);
  if (undocurr == undobuffer.length())
    undobuffer.add(copy);
  else
    undobuffer[undocurr] = copy;
  undocurr++;
}

void setcube(const vec3i &xyz, const world::brickcube &c, bool undoable) {
  if (world::getcube(xyz) == c) return;
  if (undoable) saveundocube(xyz);
  world::setcube(xyz, c);
}

static void set(const vec3i &xyz, const world::brickcube &c, bool undoable = true) {
  edit::setcube(xyz, c, undoable);
  client::addmsg(1, 14, SV_CUBE, xyz.x, xyz.y, xyz.z, c.p.x, c.p.y, c.p.z, c.mat,
                 c.tex[0], c.tex[1], c.tex[2], c.tex[3], c.tex[4], c.tex[5]);
}

static void switchcubes(int which) {
  auto old = undobuffer[which];
  undobuffer[which] = clone(world::getcube(old.xyz),old.xyz,old.action);
  set(old.xyz, old.c, false);
}

static void undo(void) {
  EDIT
  if (undocurr == 0 || undobuffer.length() == 0) {
    console::out("nothing to undo");
    return;
  }
  int last = undocurr-1;
  const int action = undobuffer[last].action;
  for (;;) {
    if (undobuffer[last].action != action) break;
    switchcubes(last);
    last = --undocurr-1;
    if (last == -1) break;
  }
}
COMMAND(undo, ARG_NONE);

static void redo(void) {
  EDIT
  if (undocurr == undobuffer.length()) {
    console::out("nothing to redo");
    return;
  }
  const int action = undobuffer[undocurr].action;
  for (;;) {
    switchcubes(undocurr);
    if (++undocurr == undobuffer.length() || undobuffer[undocurr].action != action)
      break;
  }
}
COMMAND(redo, ARG_NONE);

/*-------------------------------------------------------------------------
 - copy / paste
 -------------------------------------------------------------------------*/
static vector<clone> copies;

static void copy(void) {
  EDITSEL
  copies.clear();
  const vec3i m = min(cubestart, cubeend);
  const vec3i M = max(cubestart, cubeend);
  loopxyz(m,M+vec3i(two),copies.add(clone(world::getcube(xyz),xyz-m,0,any(xyz>M))));
}
COMMAND(copy, ARG_NONE);

static void paste(void) {
  EDITSEL
  newundobuffer();
  loopv(copies) {
    auto c = copies[i].c;
    if (copies[i].onlypos) c.mat = world::getcube(copies[i].xyz+cubestart).mat;
    set(copies[i].xyz+cubestart,c);
  }
}
COMMAND(paste, ARG_NONE);

/*-------------------------------------------------------------------------
 - draw stuff to help with editing
 -------------------------------------------------------------------------*/
struct selgrid { vec3i start, end; u16 n, face; };
static selgrid getselgrid(void) {
  selgrid grid;
  grid.start = editcorner?cornerstart:cubestart;
  grid.end = dragging?(editcorner?undercursorcorner:undercursorcube):
    (editcorner?cornerend:cubeend);
  const auto m = min(grid.start, grid.end);
  const auto M = max(grid.start, grid.end);
  const u32 cn = selectedface/2;
  grid.start = m;
  grid.end = M;
  grid.n = cn;
  grid.face = selectedface;
  return grid;
}

static void drawselgrid(void) {
  auto grid = getselgrid();
  const u32 cn = grid.n;
  grid.start[cn] = grid.end[cn] = selectedface%2?grid.end[cn]:grid.start[cn];
  const u32 delta = grid.face%2?(disttoselected+1):-disttoselected;
  grid.start[cn] += delta;
  grid.end[cn] += delta;
  grid.end[(cn+1)%3]++;
  grid.end[(cn+2)%3]++;
  vector<vec3f> lines;
  const u32 other[] = {(cn+1)%3,(cn+2)%3};
  for (int i = 0; i < 2; ++i) {
    const u32 c = other[i];
    for (int j = grid.start[c]; j <= grid.end[c]; ++j) {
      vec3f start = vec3f(grid.start), end = vec3f(grid.end);
      start[c] = end[c] = float(j);
      lines.add(vec3f(start).xzy());
      lines.add(vec3f(end).xzy());
    }
  }
  ogl::bindshader(ogl::COLOR_ONLY);
  OGL(VertexAttrib3fv, ogl::COL, &red.x);
  ogl::immdraw(GL_LINES, 3, 0, 0, lines.length(), &lines[0].x);
}

static void drawselectedcorners(void) {
  auto grid = getselgrid();
  vector<vec3f> lines;
  auto docube = [&](vec3i xyz) {
    for (u32 i=0; i<ARRAY_ELEM_N(cubeedges); ++i) {
      const auto p = world::getpos(xyz);
      const int e0 = cubeedges[i].x, e1 = cubeedges[i].y;
      lines.add((vec3f(p)+.2f*cubefverts[e0]-vec3f(.1f)).xzy()); // length of edges == 0.2
      lines.add((vec3f(p)+.2f*cubefverts[e1]-vec3f(.1f)).xzy());
    }
  };
  loopxyz(grid.start, grid.end+vec3i(one), docube(xyz));
  ogl::bindshader(ogl::COLOR_ONLY);
  OGL(VertexAttrib3fv, ogl::COL, &yellow.x);
  ogl::immdraw(GL_LINES, 3, 0, 0, lines.length(), &lines[0].x);
}

static void drawselbox(void) {
  const auto grid = getselgrid();
  const vec3f start(grid.start.xzy()), end(grid.end.xzy()+vec3i(one));
  rr::box(start, end-start, white);
}

static void drawcursorface(vec3f xyz, vec3f col, int face) {
  const vec3f fv[] = {
    (cubefverts[cubequads[face][0]]+xyz).xzy(),
    (cubefverts[cubequads[face][1]]+xyz).xzy(),
    (cubefverts[cubequads[face][2]]+xyz).xzy(),
    (cubefverts[cubequads[face][3]]+xyz).xzy()
  };
  OGL(VertexAttrib3fv, ogl::COL, &col.x);
  ogl::immdraw(GL_LINE_LOOP, 3, 0, 0, ARRAY_ELEM_N(fv), &fv[0][0]);
}

static int closerface(vec3f p, vec3i xyz) {
  const vec3f d0 = abs(p-vec3f(xyz));
  const vec3f d1 = abs(p-vec3f(xyz+vec3i(one)));
  int closer = 0;
  float m = d0.x;
  if (d1.x < m) { closer=1; m=d1.x; }
  if (d0.y < m) { closer=2; m=d0.y; }
  if (d1.y < m) { closer=3; m=d1.y; }
  if (d0.z < m) { closer=4; m=d0.z; }
  if (d1.z < m) closer=5;
  return closer;
}

static vec3i closercorner(vec3f p, vec3i xyz) {
  vec3i closer(zero);
  float closerd = FLT_MAX;
  const auto face = cubequads[selectedface];
  loopi(4) {
    const auto v = xyz+cubeiverts[face[i]];
    const float d = distance(p,vec3f(v));
    if (d < closerd) {
      closerd = d;
      closer = v;
    }
  }
  return closer;
}

/*-------------------------------------------------------------------------
 - editing itself
 -------------------------------------------------------------------------*/
void cursorupdate(void) { // called every frame from hud
  const auto r = mat3x3f::rotate(zaxis,game::player1->yaw)*
                 mat3x3f::rotate(yaxis,game::player1->roll)*
                 mat3x3f::rotate(-xaxis,game::player1->pitch);
  const camera cam(game::player1->o, -r.vz, -r.vy, 90.f, 1.f);
  const auto ray = cam.generate(2,2,1,1); // center of screen
  const auto res = world::castray(ray);
  anyundercursor = res.isec;
  OGL(LineWidth, 2.f);
  OGL(DepthFunc, GL_LEQUAL);
  if (res.isec) {
    const float bias = 0.1f;
    const auto exact = ray.org + res.t*ray.dir;
    const auto p = floor(exact+bias*ray.dir);
    undercursorcube = vec3i(p+vec3f(bias));
    const auto q = p.xzy();
    const int face = closerface(exact.xzy(), undercursorcube.xzy());
    undercursorface = closerface(exact, undercursorcube);
    undercursorcorner = closercorner(exact, undercursorcube);
    rr::box(vec3i(q), vec3i(one), vec3f(one)); // cube under cursor
    drawcursorface(p, blue, face);
  }
  if (anyselected) {
    if (editcorner) {
      drawselectedcorners();
    } else {
      drawselbox();
      drawselgrid();
    }
  }
  OGL(DepthFunc, GL_LESS);
  OGL(LineWidth, 1.f);
}

static vec3i extrusionvector(int face) {
  const int extrd = face/2; // x (0), y(1) or z(2)
  vec3i extrv = extrd==0 ? vec3i(1,0,0) : extrd==1 ? vec3i(0,1,0) : vec3i(0,0,1);
  return face%2==0 ? -extrv : extrv;
}

static void editcube(int dir) {
  const auto extr = extrusionvector(selectedface);
  const auto mat = dir==1 ? world::FULL : world::EMPTY;
  if (dir==1) disttoselected++;
  const auto grid = getselgrid();
  loopxyz(grid.start, grid.end+vec3i(one),
    const auto idx = extr*disttoselected+xyz;
    auto c = world::getcube(idx);
    c.mat = mat;
    set(idx, c));
  if (dir!=1) disttoselected--;
}

static void editvertex(int dir) {
  const auto grid = getselgrid();
  loopxyz(grid.start, grid.end+vec3i(one), {
    auto n = dir==1?cubenorms[selectedface]:-cubenorms[selectedface];
    auto c = world::getcube(xyz);
    const vec3i p = clamp(vec3i(c.p)+16*n, vec3i(-128), vec3i(127));
    c.p = vec3<s8>(p);
    set(xyz, c);
  });
}

static void symmetry(bool isdown) {
  EDITSEL
  if (!isdown) return;
  newundobuffer();
  const u32 symmetryaxis=symmetrydir/2;
  const auto m = min(cubestart, cubeend);
  const auto M = max(cubestart, cubeend);
  loopxyz(m,M+vec3i(two),saveundocube(xyz));
  auto doswappos = [&](const vec3i &xyz) {
    vec3i idx0(xyz), idx1(xyz);
    idx1[symmetryaxis] = M[symmetryaxis]-xyz[symmetryaxis]+m[symmetryaxis]+1;
    auto c0 = world::getcube(idx0), c1 = world::getcube(idx1);
    const auto p = c1.p;
    c1.p = c0.p;
    c0.p = p;
    c1.p[symmetryaxis] = -1-c1.p[symmetryaxis];
    c0.p[symmetryaxis] = -1-c0.p[symmetryaxis];
    set(idx0, c0);
    set(idx1, c1);
  };
  auto doswapmat = [&](const vec3i &xyz) {
    auto idx0 = xyz, idx1 = xyz;
    idx1[symmetryaxis] = M[symmetryaxis]-xyz[symmetryaxis]+m[symmetryaxis];
    auto c0 = world::getcube(idx0), c1 = world::getcube(idx1);
    const auto p0 = c0.p, p1 = c1.p;
    c0.p = p1; c1.p = p0; // do not swap positions back
    set(idx0, c1);
    set(idx1, c0);
  };
  auto end = M+vec3i(one);
  end[symmetryaxis] = (M[symmetryaxis]+m[symmetryaxis])/2+1;
  loopxyz(m,end,doswapmat(xyz));
  end = M+vec3i(2);
  end[symmetryaxis] = (M[symmetryaxis]+m[symmetryaxis]+1)/2+1;
  loopxyz(m,end,doswappos(xyz));
  symmetrydir = (symmetrydir+1)%6;
}
COMMAND(symmetry, ARG_DOWN);

static void editaction(int dir) { // +1 or -1
  EDITSEL
  newundobuffer();
  if (editcorner) editvertex(dir); else editcube(dir);
}
COMMAND(editaction, ARG_1INT);

static void edittex(int dir) { // +1 or -1
  EDITSEL;
  newundobuffer();
  const auto m = min(cubestart, cubeend);
  const auto M = max(cubestart, cubeend) + vec3i(one);
  auto doupdatetex = [&](const vec3i &xyz) {
    auto c = world::getcube(xyz);
    c.tex[selectedface] = (c.tex[selectedface] + dir) % ogl::MAXMAPTEX;
    world::setcube(xyz,c);
  };
  loopxyz(m,M,doupdatetex(xyz));
}
COMMAND(edittex, ARG_1INT);

#undef EDIT
#undef EDITSEL

} // namespace edit
} // namespace cube

