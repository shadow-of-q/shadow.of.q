#include "cube.hpp"

namespace cube {
namespace ogl { void buildgrid(); }// XXX
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

static void drawcursorface(vec3f xyz, vec3f col, int face) {
  const vec3f fv[] = {
    cubefverts[cubequads[face][0]]+xyz,
    cubefverts[cubequads[face][1]]+xyz,
    cubefverts[cubequads[face][2]]+xyz,
    cubefverts[cubequads[face][3]]+xyz
  };
  OGL(VertexAttrib3fv, ogl::COL, &col.x);
  ogl::immdraw(GL_LINE_LOOP, 3, 0, 0, ARRAY_ELEM_N(fv), &fv[0][0]);
}

static bool anyundercursor = false; // do we see anything on the center of screen?
static vec3i undercursor(zero); // cube under the cursor
static int undercursorface = 0; // which face we specifically select (0 to 5)

static bool anyselected = false; // curr selection (face, start and end cubes)
static vec3i selectedstart(zero), selectedend(zero);
static int selectedface = 0, disttoselected = 0;

static bool dragging = false;
void editdrag(bool isdown) {
  if (isdown) {
    dragging = anyselected = anyundercursor;
    selectedstart = selectedend = undercursor;
    selectedface = undercursorface;
    disttoselected = 0;
  }
  if (!isdown) {
    dragging = false;
    selectedend = undercursor;
  }
}

struct selectiongrid { vec3i start, end; u16 n, face; };
static selectiongrid getselectiongrid(void) {
  selectiongrid grid;
  grid.start = selectedstart;
  grid.end = dragging?undercursor:selectedend;
  const auto m = min(grid.start, grid.end);
  const auto M = max(grid.start, grid.end);
  const u32 cn = selectedface/2;
  grid.start = m;
  grid.end = M;
  grid.n = cn;
  grid.face = selectedface;
  return grid;
}

static void drawselectiongrid(void) {
  auto grid = getselectiongrid();
  const int cn = grid.n;
  grid.start[cn] = grid.end[cn] = selectedface%2?grid.end[cn]:grid.start[cn];
  const int delta = grid.face%2?(disttoselected+1):-disttoselected;
  grid.start[grid.n] += delta;
  grid.end[grid.n] += delta;
  grid.end[(grid.n+1)%3]++;
  grid.end[(grid.n+2)%3]++;
  vector<vec3f> lines;
  for (int i = 0; i < 2; ++i) {
    const u32 c = (grid.n+i+1)%3;
    for (int j = grid.start[c]; j <= grid.end[c]; ++j) {
      vec3i start = grid.start, end = grid.end;
      start[c] = end[c] = j;
      lines.add(vec3f(start).xzy());
      lines.add(vec3f(end).xzy());
    }
  }
  ogl::bindshader(ogl::COLOR_ONLY);
  OGL(VertexAttrib3f, ogl::COL, 1.f,0.f,0.f);
  ogl::immdraw(GL_LINES, 3, 0, 0, lines.length(), &lines[0].x);
}

static void drawselectionbox(void) {
  const auto grid = getselectiongrid();
  const vec3f start(grid.start.xzy()), end(grid.end.xzy()+vec3i(one));
  rr::box(start, end-start, vec3f(1.f,1.f,1.f));
}

void cursorupdate(void) { // called every frame from hud
  const auto r = mat3x3f::rotate(vec3f(0.f,0.f,1.f),game::player1->yaw)*
                 mat3x3f::rotate(vec3f(0.f,1.f,0.f),game::player1->roll)*
                 mat3x3f::rotate(vec3f(-1.f,0.f,0.f),game::player1->pitch);
  const camera cam(game::player1->o, -r.vz, -r.vy, 90.f, 1.f);
  const auto ray = cam.generate(2,2,1,1); // center of screen
  const auto res = world::castray(ray);
  anyundercursor = res.isec;
  if (!res.isec) return;
  const float bias = 0.1f;
  const auto exact = ray.org + res.t*ray.dir;
  const auto p = floor(exact+bias*ray.dir);
  undercursor = vec3i(p+vec3f(bias));
  const auto q = p.xzy();
  const int face = closerface(exact.xzy(), undercursor.xzy());
  undercursorface = closerface(exact, undercursor);
  OGL(LineWidth, 2.f);
  OGL(DepthFunc, GL_LEQUAL);
  rr::box(vec3i(q).xzy(), vec3i(one), vec3f(one)); // cube under cursor
  drawcursorface(q, vec3f(0.f,0.f,1.f), face);
  if (anyselected) {
    drawselectionbox();
    drawselectiongrid();
  }
  OGL(DepthFunc, GL_LESS);
  OGL(LineWidth, 1.f);
}

static vec3i extrusionvector(int face) {
  const int extrd = face/2; // x (0), y(1) or z(2)
  vec3i extrv = extrd==0 ? vec3i(1,0,0) : extrd==1 ? vec3i(0,1,0) : vec3i(0,0,1);
  return face%2==0 ? -extrv : extrv;
}

static void editcube(int dir) { // +1 or -1
  using namespace world;
  if (!editmode || !anyselected) return;
  const auto extr = extrusionvector(selectedface);
  const auto mat = dir==1 ? FULL : EMPTY;
  if (dir==1) disttoselected++;
  const auto grid = getselectiongrid();
  loopxyz(grid.start, grid.end+vec3i(one), setcube(extr*disttoselected+xyz, brickcube(mat)));
  if (dir!=1) disttoselected--;
  ogl::buildgrid();
}
COMMAND(editcube, ARG_1INT);

} // namespace edit
} // namespace cube

