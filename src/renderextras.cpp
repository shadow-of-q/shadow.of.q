#include "cube.hpp"

namespace cube {
namespace rr {

void line(int x1, int y1, float z1, int x2, int y2, float z2) {
  const arrayf<3> verts[] = {
    arrayf<3>(float(x1), z1, float(y1)),
    arrayf<3>(float(x1), z1, float(y1)+0.01f),
    arrayf<3>(float(x2), z2, float(y2)),
    arrayf<3>(float(x2), z2, float(y2)+0.01f)
  };
  ogl::bindshader(ogl::COLOR);
  ogl::immdraw(GL_TRIANGLE_STRIP, 3, 0, 0, 4, &verts[0][0]);
  ogl::xtraverts += 4;
}

void linestyle(float width, int r, int g, int b) {
  OGL(LineWidth, width);
  OGL(VertexAttrib3f,ogl::COL,float(r)/255.f,float(g)/255.f,float(b)/255.f);
}

void box(const vec3i &start, const vec3i &size, const vec3f &col) {
  const vec3f fstart(start), fsize(size);
  vec3f v[2*ARRAY_ELEM_N(cubeedges)];
  loopi(int(ARRAY_ELEM_N(cubeedges))) {
    v[2*i+0] = fsize*cubefverts[cubeedges[i].x]+fstart;
    v[2*i+1] = fsize*cubefverts[cubeedges[i].y]+fstart;
  }
  ogl::bindshader(ogl::COLOR);
  OGL(VertexAttrib3fv, ogl::COL, &col.x);
  ogl::immdraw(GL_LINES, 3, 0, 0, ARRAY_ELEM_N(v), &v[0][0]);
}

void dot(int x, int y, float z) {
  const float DOF = 0.1f;
  const arrayf<3> verts[] = {
    arrayf<3>(x-DOF, float(z), y-DOF),
    arrayf<3>(x+DOF, float(z), y-DOF),
    arrayf<3>(x+DOF, float(z), y+DOF),
    arrayf<3>(x-DOF, float(z), y+DOF)
  };
  ogl::bindshader(ogl::COLOR);
  ogl::immdraw(GL_LINE_LOOP, 3, 0, 0, 4, &verts[0][0]);
  ogl::xtraverts += 4;
}

void blendbox(int x1, int y1, int x2, int y2, bool border) {
  OGL(DepthMask, GL_FALSE);
  OGL(BlendFunc, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
  ogl::setattribarray()(ogl::POS0);
  ogl::bindshader(ogl::COLOR);
  if (border)
    OGL(VertexAttrib3f, ogl::COL, .5f, .3f, .4f);
  else
    OGL(VertexAttrib3f, ogl::COL, 1.f, 1.f, 1.f);

  const arrayf<2> verts0[] = {
    arrayf<2>(float(x1), float(y1)),
    arrayf<2>(float(x2), float(y1)),
    arrayf<2>(float(x1), float(y2)),
    arrayf<2>(float(x2), float(y2))
  };
  ogl::immdraw(GL_TRIANGLE_STRIP, 2, 0, 0, 4, &verts0[0][0]);

  ogl::disablev(GL_BLEND);
  OGL(VertexAttrib3f, ogl::COL, .2f, .7f, .4f);
  const arrayf<2> verts1[] = {
    arrayf<2>(float(x1), float(y1)),
    arrayf<2>(float(x2), float(y1)),
    arrayf<2>(float(x2), float(y2)),
    arrayf<2>(float(x1), float(y2))
  };
  ogl::immdraw(GL_LINE_LOOP, 2, 0, 0, 4, &verts1[0][0]);

  OGL(DepthMask, GL_TRUE);
  ogl::enablev(GL_BLEND);
  ogl::xtraverts += 8;
}

static const int MAXSPHERES = 50;
struct sphere { vec3f o; float size, max; int type; sphere *next; };
static sphere spheres[MAXSPHERES], *slist = NULL, *sempty = NULL;
static bool sinit = false;

void newsphere(const vec3f &o, float max, int type) {
  if (!sinit) {
    loopi(MAXSPHERES) {
      spheres[i].next = sempty;
      sempty = &spheres[i];
    }
    sinit = true;
  }
  if (sempty) {
    sphere *p = sempty;
    sempty = p->next;
    p->o = o;
    p->max = max;
    p->size = 1;
    p->type = type;
    p->next = slist;
    slist = p;
  }
}

void renderspheres(int time) {
  ogl::enablev(GL_BLEND);
  OGL(DepthMask, GL_FALSE);
  OGL(BlendFunc, GL_SRC_ALPHA, GL_ONE);
  ogl::bindgametexture(GL_TEXTURE_2D, 4);

  for (sphere *p, **pp = &slist; (p = *pp);) {
    const float size = p->size/p->max;
    ogl::pushmatrix();
    OGL(VertexAttrib4f, ogl::COL, 1.0f, 1.0f, 1.0f, 1.0f-size);
    ogl::translate(vec3f(p->o.x, p->o.z, p->o.y));
    ogl::rotate(game::lastmillis()/5.0f, vec3f(one));
    ogl::scale(vec3f(p->size));
    ogl::drawsphere();
    ogl::scale(vec3f(0.8f));
    ogl::drawsphere();
    ogl::popmatrix();
    ogl::xtraverts += 12*6*2;

    if (p->size>p->max) {
      *pp = p->next;
      p->next = sempty;
      sempty = p;
    } else {
      p->size += time/100.0f;
      pp = &p->next;
    }
  }

  ogl::disablev(GL_BLEND);
  OGL(DepthMask, GL_TRUE);
}

static string closeent;

// show sparkly thingies for map entities in edit mode
void renderents(void) {
  closeent[0] = 0;
  if (!edit::mode()) return;
  loopv(game::ents) {
    game::entity &e = game::ents[i];
    if (e.type==game::NOTUSED) continue;
    vec3f v(float(e.x), float(e.y), float(e.z));
    particle_splash(2, 2, 40, v);
  }
  const int e = world::closestent();
  if (e>=0) {
    game::entity &c = game::ents[e];
    sprintf_s(closeent)
      ("closest entity = %s (%d, %d, %d, %d), selection = (%d, %d)",
      game::entnames(c.type), c.attr1, c.attr2, c.attr3, c.attr4,
      cmd::getvar("selxs"), cmd::getvar("selys"));
  }
}

static void loadsky(char *basename) {
  static string lastsky = "";
  if (strcmp(lastsky, basename)==0) return;
  const char *side[] = { "ft", "bk", "lf", "rt", "dn", "up" };
  const int texnum = 14;
  loopi(6) {
    sprintf_sd(name)("packages/%s_%s.jpg", basename, side[i]);
    int xs, ys;
    if (!ogl::installtex(texnum+i, path(name), xs, ys, true))
      console::out("could not load sky textures");
  }
  strcpy_s(lastsky, basename);
}

COMMAND(loadsky, ARG_1STR);

static float cursordepth = 0.9f;
static vec4i viewport;
static mat4x4f readmm, readpm;

static void readmatrices(void) {
  OGL(GetIntegerv, GL_VIEWPORT, &viewport.x);
  readmm = ogl::matrix(ogl::MODELVIEW);
  readpm = ogl::matrix(ogl::PROJECTION);
}

static float depthcorrect(float d) { return (d<=1/256.0f) ? d*256 : d; }

void readdepth(int w, int h) {
#if !defined(EMSCRIPTEN)
  OGL (ReadPixels, w/2, h/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &cursordepth);
#else
  cursordepth = 1.f;
#endif
  const vec3f screenp(float(w)/2.f,float(h)/2.f,depthcorrect(cursordepth));
  const vec3f v = unproject(screenp, readmm, readpm, viewport);
  game::setworldpos(vec3f(v.x,v.z,v.y));
  const vec3f r(readmm.vx.x,readmm.vy.x,readmm.vz.x);
  const vec3f u(readmm.vx.y,readmm.vy.y,readmm.vz.y);
  setorient(r, u);
}

void drawicon(float tx, float ty, int x, int y) {
  const float o = 1/3.0f;
  const int s = 120;
  tx /= 192;
  ty /= 192;
  ogl::bindgametexture(GL_TEXTURE_2D, 5);
  const arrayf<4> verts[] = {
    arrayf<4>(tx,   ty,   x,   y),
    arrayf<4>(tx+o, ty,   x+s, y),
    arrayf<4>(tx,   ty+o, x,   y+s),
    arrayf<4>(tx+o, ty+o, x+s, y+s)
  };
  ogl::bindshader(ogl::DIFFUSETEX);
  ogl::immdraw(GL_TRIANGLE_STRIP, 2, 2, 0, 4, &verts[0][0]);
  ogl::xtraverts += 4;
}

static void invertperspective(void) {
  const mat4x4f &pm = ogl::matrix(ogl::PROJECTION); // must be perspective
  mat4x4f inv(zero);
  inv.vx.x = 1.f/pm.vx.x;
  inv.vy.y = 1.f/pm.vy.y;
  inv.vz.w = 1.f/pm.vw.z;
  inv.vw.z = -1.f;
  inv.vw.w = pm.vz.z/pm.vw.z;
  ogl::loadmatrix(inv);
}

static int dblend = 0;
void damageblend(int n) { dblend += n; }

VARP(crosshairsize, 0, 15, 50);
VAR(hidestats, 0, 0, 1);
VARP(crosshairfx, 0, 1, 1);

void drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater) {
  readmatrices();
  if (edit::mode()) {
    if (cursordepth==1.0f) game::setworldpos(game::player1->o);
    edit::cursorupdate();
  }
  ogl::disablev(GL_DEPTH_TEST);
  invertperspective();
  ogl::pushmatrix();
  ogl::ortho(0.f, float(VIRTW), float(VIRTH), 0.f, -1.f, 1.f);
  ogl::enablev(GL_BLEND);

  OGL(DepthMask, GL_FALSE);
  if (dblend || underwater) {
    OGL(BlendFunc, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    const arrayf<2> verts[] = {
      arrayf<2>(0.f,          0.f),
      arrayf<2>(float(VIRTW), 0.f),
      arrayf<2>(0.f,          float(VIRTH)),
      arrayf<2>(float(VIRTW), float(VIRTH))
    };
    if (dblend)
      OGL(VertexAttrib3f,ogl::COL,0.0f,0.9f,0.9f);
    else
      OGL(VertexAttrib3f,ogl::COL,0.9f,0.5f,0.0f);
    ogl::bindshader(ogl::COLOR);
    ogl::immdraw(GL_TRIANGLE_STRIP,2,0,0,4,&verts[0][0]);
    dblend -= game::curtime()/3;
    if (dblend<0) dblend = 0;
  }

  const char *command = console::getcurcommand();
  const char *player = game::playerincrosshair();
  if (command)
    draw_textf("> %s_", 20, 1570, 2, command);
  else if (closeent[0])
    draw_text(closeent, 20, 1570, 2);
  else if (player)
    draw_text(player, 20, 1570, 2);

  game::renderscores();
  if (!menu::render()) {
    OGL(BlendFunc, GL_SRC_ALPHA, GL_SRC_ALPHA);
    ogl::bindgametexture(GL_TEXTURE_2D, 1);
    OGL(VertexAttrib3f,ogl::COL,1.f,1.f,1.f);
    if (crosshairfx) {
      if (game::player1->gunwait)
        OGL(VertexAttrib3f,ogl::COL,0.5f,0.5f,0.5f);
      else if (game::player1->health<=25)
        OGL(VertexAttrib3f,ogl::COL,1.0f,0.0f,0.0f);
      else if (game::player1->health<=50)
        OGL(VertexAttrib3f,ogl::COL,1.0f,0.5f,0.0f);
    }
    const float csz = float(crosshairsize);
    const arrayf<4> verts[] = {
      arrayf<4>(0.f, 0.f, float(VIRTW/2) - csz, float(VIRTH/2) - csz),
      arrayf<4>(1.f, 0.f, float(VIRTW/2) + csz, float(VIRTH/2) - csz),
      arrayf<4>(0.f, 1.f, float(VIRTW/2) - csz, float(VIRTH/2) + csz),
      arrayf<4>(1.f, 1.f, float(VIRTW/2) + csz, float(VIRTH/2) + csz)
    };
    ogl::bindshader(ogl::DIFFUSETEX);
    ogl::immdraw(GL_TRIANGLE_STRIP, 2, 2, 0, 4, &verts[0][0]);
  }
  ogl::popmatrix();

  ogl::pushmatrix();
  ogl::ortho(0.f,VIRTW*4.f/3.f,VIRTH*4.f/3.f,0.f,-1.f,1.f);
  console::render();

  if (!hidestats) {
    const vec3f &o = game::player1->o;
    ogl::popmatrix();
    ogl::pushmatrix();
    ogl::ortho(0.f,VIRTW*3.f/2.f,VIRTH*3.f/2.f,0.f,-1.f,1.f);
    draw_textf("pos %d %d %d", 3100, 2320, 2, int(o.x), int(o.y), int(o.z));
    draw_textf("fps %d", 3000, 2390, 2, curfps);
    draw_textf("wqd %d", 3000, 2460, 2, nquads);
    draw_textf("wvt %d", 3000, 2530, 2, curvert);
    draw_textf("evt %d", 3000, 2600, 2, ogl::xtraverts);
  }

  ogl::popmatrix();

  if (game::player1->state==CS_ALIVE) {
    ogl::pushmatrix();
    ogl::ortho(0, VIRTW/2, VIRTH/2, 0, -1, 1);
    draw_textf("%d",  90, 827, 2, game::player1->health);
    if (game::player1->armour) draw_textf("%d", 390, 827, 2, game::player1->armour);
    draw_textf("%d", 690, 827, 2, game::player1->ammo[game::player1->gunselect]);
    ogl::popmatrix();
    ogl::pushmatrix();
    ogl::ortho(0.f, float(VIRTW), float(VIRTH), 0.f, -1.f, 1.f);
    ogl::disablev(GL_BLEND);
    drawicon(128, 128, 20, 1650);
    if (game::player1->armour) drawicon((float)(game::player1->armourtype*64), 0, 620, 1650);
    int g = game::player1->gunselect;
    int r = 64;
    if (g>2) { g -= 3; r = 128; }
    drawicon((float)(g*64), (float)r, 1220, 1650);
    ogl::popmatrix();
  }

  OGL(DepthMask, GL_TRUE);
  ogl::disablev(GL_BLEND);
  ogl::enablev(GL_DEPTH_TEST);
}

} // namespace rr
} // namespace cube

