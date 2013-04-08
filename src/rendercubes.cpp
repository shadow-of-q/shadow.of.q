#include "cube.h"
#include "ogl.hpp"

namespace cube {
namespace rr {

vertex *verts = NULL;
int curvert = 4;
int curmaxverts = 10000;

void setarraypointers(void)
{
  OGL(VertexAttribPointer, ogl::POS0, 3, GL_FLOAT, 0, sizeof(vertex),(const void*)offsetof(vertex,x));
  OGL(VertexAttribPointer, ogl::COL, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex),(const void*)offsetof(vertex,r));
  OGL(VertexAttribPointer, ogl::TEX, 2, GL_FLOAT, 0, sizeof(vertex), (const void*)offsetof(vertex,u));
}
int worldsize(void) { return curvert*sizeof(vertex); }

} // namespace rr
} // namespace cube

