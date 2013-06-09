#include "cube.hpp"

namespace cube {
namespace rr {

static const int MAXPARTICLES = 10500;
static const int NUMPARTCUTOFF = 20;
struct particle { vec3f o, d; int fade, type; int millis; particle *next; };
static particle particles[MAXPARTICLES], *parlist = NULL, *parempty = NULL;
static bool parinit = false;

VARP(maxparticles, 100, 2000, MAXPARTICLES-500);
VAR(demotracking, 0, 0, 1);
VARP(particlesize, 20, 100, 500);

static void newparticle(const vec3f &o, const vec3f &d, int fade, int type) {
  if (!parinit) {
    loopi(MAXPARTICLES) {
      particles[i].next = parempty;
      parempty = &particles[i];
    }
    parinit = true;
  }
  if (parempty) {
    particle *p = parempty;
    parempty = p->next;
    p->o = o;
    p->d = d;
    p->fade = fade;
    p->type = type;
    p->millis = game::lastmillis();
    p->next = parlist;
    parlist = p;
  }
}

static vec3f right, up;

void setorient(const vec3f &r, const vec3f &u) { right = r; up = u; }

static const struct parttype {vec3f rgb; int gr, tex; float sz;} parttypes[] = {
  {vec3f(0.7f, 0.6f, 0.3f), 2,  3, 0.06f}, // yellow: sparks
  {vec3f(0.5f, 0.5f, 0.5f), 20, 7, 0.15f}, // grey:   small smoke
  {vec3f(0.2f, 0.2f, 1.0f), 20, 3, 0.08f}, // blue:   edit mode entities
  {vec3f(1.0f, 0.1f, 0.1f), 1,  7, 0.06f}, // red:    blood spats
  {vec3f(1.0f, 0.8f, 0.8f), 20, 6, 1.2f }, // yellow: fireball1
  {vec3f(0.5f, 0.5f, 0.5f), 20, 7, 0.6f }, // grey:   big smoke
  {vec3f(1.0f, 1.0f, 1.0f), 20, 8, 1.2f }, // blue:   fireball2
  {vec3f(1.0f, 1.0f, 1.0f), 20, 9, 1.2f }, // green:  fireball3
  {vec3f(1.0f, 0.1f, 0.1f), 0,  7, 0.2f }, // red:    demotrack
};
static const int parttypen = ARRAY_ELEM_N(parttypes);

typedef array<float,8> glparticle; // TODO use a more compressed format than that
static GLuint particleibo = 0u, particlevbo = 0u;
static const int glindexn = 6*MAXPARTICLES, glvertexn = 4*MAXPARTICLES;
//static glparticle glparts[glvertexn];
static glparticle *glparts = NULL;

static void initparticles(void) {
  // indices never change we set them once here
  const u16 twotriangles[] = {0,1,2,2,3,1};
  u16 *indices = NEWAE(u16, glindexn);
  ogl::genbuffers(1, &particleibo);
  ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, particleibo);
  loopi(MAXPARTICLES) loopj(6) indices[6*i+j]=4*i+twotriangles[j];
  OGL(BufferData, GL_ELEMENT_ARRAY_BUFFER, glindexn*sizeof(u16), indices, GL_STATIC_DRAW);
  ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, 0);

  // vertices will be created at each drawing call
  ogl::genbuffers(1, &particlevbo);
  ogl::bindbuffer(ogl::ARRAY_BUFFER, particlevbo);
  OGL(BufferData, GL_ARRAY_BUFFER, glvertexn*sizeof(glparticle), NULL, GL_DYNAMIC_DRAW);
  ogl::bindbuffer(ogl::ARRAY_BUFFER, 0);
  SAFE_DELETEA(indices);

  // Init the array dynamically since VS2012 crashes with a static declaration
  glparts = NEWAE(glparticle, glvertexn);
}

void cleanparticles(void) {
  if (particleibo) {
    ogl::deletebuffers(1, &particleibo);
    particleibo = 0;
  }
  if (particlevbo) {
    ogl::deletebuffers(1, &particlevbo);
    particlevbo = 0;
  }
  SAFE_DELETEA(glparts);
}
void render_particles(int time) {
  if (demo::playing() && demotracking) {
    const vec3f nom(0, 0, 0);
    newparticle(game::player1->o, nom, 100000000, 8);
  }
  if (particleibo == 0u) initparticles();

  // bucket sort the particles
  uint partbucket[parttypen], partbucketsize[parttypen];
  loopi(parttypen) partbucketsize[i] = 0;
  for (particle *p, **pp = &parlist; (p = *pp);) {
    pp = &p->next;
    partbucketsize[p->type]++;
  }
  partbucket[0] = 0;
  loopi(parttypen-1) partbucket[i+1] = partbucket[i]+partbucketsize[i];

  // copy the particles to the vertex buffer
  int numrender = 0;
  for (particle *p, **pp = &parlist; (p = *pp);) {
    const uint index = 4*partbucket[p->type]++;
    const parttype *pt = &parttypes[p->type];
    const float sz = pt->sz*particlesize/100.0f;
    const vec3f poxzy = p->o.xzy();
    glparts[index+0] = glparticle(pt->rgb, 0.f, 1.f, poxzy-(right-up)*sz);
    glparts[index+1] = glparticle(pt->rgb, 1.f, 1.f, poxzy+(right+up)*sz);
    glparts[index+2] = glparticle(pt->rgb, 0.f, 0.f, poxzy-(right+up)*sz);
    glparts[index+3] = glparticle(pt->rgb, 1.f, 0.f, poxzy-(up-right)*sz);
	if (numrender++>maxparticles || (p->fade -= time)<0) {
      *pp = p->next;
      p->next = parempty;
      parempty = p;
    } else {
      if (pt->gr) p->o.z -= ((game::lastmillis()-p->millis)/3.0f)*game::curtime()/(pt->gr*10000);
      vec3f a = p->d;
      a *= float(time);
      a /= 20000.f;
      p->o += a;
      pp = &p->next;
    }
  }

  // render all of them now
  partbucket[0] = 0;
  loopi(parttypen-1) partbucket[i+1] = partbucket[i]+partbucketsize[i];
  OGL(DepthMask, GL_FALSE);
  ogl::enable(GL_BLEND);
  OGL(BlendFunc, GL_SRC_ALPHA, GL_SRC_ALPHA);
  ogl::setattribarray()(ogl::POS0, ogl::COL, ogl::TEX0);
  ogl::bindbuffer(ogl::ARRAY_BUFFER, particlevbo);
  ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, particleibo);
  OGL(BufferSubData, GL_ARRAY_BUFFER, 0, numrender*sizeof(glparticle[4]), glparts);
  OGL(VertexAttribPointer, ogl::COL, 3, GL_FLOAT, 0, sizeof(glparticle), (const void*)0);
  OGL(VertexAttribPointer, ogl::TEX0, 2, GL_FLOAT, 0, sizeof(glparticle), (const void*)sizeof(float[3]));
  OGL(VertexAttribPointer, ogl::POS0, 3, GL_FLOAT, 0, sizeof(glparticle), (const void*)sizeof(float[5]));
  loopi(parttypen) {
    if (partbucketsize[i] == 0) continue;
    const parttype *pt = &parttypes[i];
    const int n = partbucketsize[i]*6;
    ogl::bindgametexture(GL_TEXTURE_2D, pt->tex);
    const void *offset = (const void *) (partbucket[i] * sizeof(u16[6]));
    ogl::drawelements(GL_TRIANGLES, n, GL_UNSIGNED_SHORT, offset);
  }

  ogl::bindbuffer(ogl::ARRAY_BUFFER, 0);
  ogl::bindbuffer(ogl::ELEMENT_ARRAY_BUFFER, 0);
  ogl::disable(GL_BLEND);
  OGL(DepthMask, GL_TRUE);
}

void particle_splash(int type, int num, int fade, const vec3f &p) {
  loopi(num) {
    const int radius = type==5 ? 50 : 150;
    int x, y, z;
    do {
      x = rnd(radius*2)-radius;
      y = rnd(radius*2)-radius;
      z = rnd(radius*2)-radius;
    } while (x*x+y*y+z*z>radius*radius);
    const vec3f d = vec3f(float(x), float(y), float(z));
    newparticle(p, d, rnd(fade*3), type);
  }
}

void particle_trail(int type, int fade, const vec3f &s, const vec3f &e) {
  const vec3f v = e-s;
  const float d = length(v);
  const vec3f nv = v/(d*2.f+0.1f);
  vec3f p = s;
  loopi(int(d)*2) {
    p += nv;
    const cube::vec3f d(float(rnd(12)-5), float(rnd(11)-5), float(rnd(11)-5));
    newparticle(p, d, rnd(fade)+fade, type);
  }
}
} // namespace rr
} // namespace cube

