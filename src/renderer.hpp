#pragma once
#include "entities.hpp"
#include "math.hpp"

namespace cube {
namespace rr {

static const int VIRTW = 2400; // screen width for text & HUD
static const int VIRTH = 1800; // screen height for text & HUD
static const int FONTH = 64; // font height
static const int PIXELTAB = VIRTW / 12; // tabulation size in pixels

// free up all resources needed by renderer
void clean(void);

// rendertext
void draw_text(const char *str, int left, int top, int gl_num);
void draw_textf(const char *fstr, int left, int top, int gl_num, ...);
int text_width(const char *str);
void draw_envbox(int t, int fogdist);

// renderextras
void line(int x1, int y1, float z1, int x2, int y2, float z2);
void box(const vec3i &start, const vec3i &size, const vec3f &col);
void dot(int x, int y, float z);
void linestyle(float width, int r, int g, int b);
void newsphere(const vec3f &o, float max, int type);
void renderspheres(int time);
void drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater);
void readdepth(int w, int h);
void blendbox(int x1, int y1, int x2, int y2, bool border);
void damageblend(int n);
void renderents(void);

// renderparticles
void setorient(const vec3f &r, const vec3f &u);
void particle_splash(int type, int num, int fade, const vec3f &p);
void particle_trail(int type, int fade, const vec3f &from, const vec3f &to);
void render_particles(int time);
void cleanparticles(void);

// rendermd2
void rendermodel(const char *mdl, int frame, int range, int tex, float rad, const vec3f &o, float yaw, float pitch, bool teammate, float scale, float speed, int snap = 0, int basetime = 0);
game::mapmodelinfo &getmminfo(int i);
void cleanmd2(void);

} // namespace rr
} // namespace cube

