#include "math.hpp"

namespace cube {

template <typename T>
mat3x3<T>::mat3x3(const vec3<T> &n) {
  vy = n;
  if (abs(n.x) >= abs(n.y)) {
    const float inv = rcp(sqrt(n.x*n.x + n.z*n.z));
    vx = vec3<T>(-n.z*inv, zero, n.x*inv);
  } else {
    const float inv = rcp(sqrt(n.y*n.y + n.z*n.z));
    vx = vec3<T>(zero, n.z*inv, -n.y*inv);
  }
  vx = normalize(vx);
  vy = normalize(vy);
  vz = cross(vy,vx);
}

template <typename T>
mat4x4<T> mat4x4<T>::inverse(void) const {
  mat4x4<T> inv;
  inv.vx.x = vy.y*vz.z*vw.w - vy.y*vz.w*vw.z - vz.y*vy.z*vw.w
           + vz.y*vy.w*vw.z + vw.y*vy.z*vz.w - vw.y*vy.w*vz.z;
  inv.vy.x = -vy.x*vz.z*vw.w + vy.x*vz.w*vw.z + vz.x*vy.z*vw.w
           - vz.x*vy.w*vw.z - vw.x*vy.z*vz.w + vw.x*vy.w*vz.z;
  inv.vz.x = vy.x*vz.y*vw.w - vy.x*vz.w*vw.y - vz.x*vy.y*vw.w
           + vz.x*vy.w*vw.y + vw.x*vy.y*vz.w - vw.x*vy.w*vz.y;
  inv.vw.x = -vy.x*vz.y*vw.z + vy.x*vz.z*vw.y + vz.x*vy.y*vw.z
           - vz.x*vy.z*vw.y - vw.x*vy.y*vz.z + vw.x*vy.z*vz.y;
  inv.vx.y = -vx.y*vz.z*vw.w + vx.y*vz.w*vw.z + vz.y*vx.z*vw.w
           - vz.y*vx.w*vw.z - vw.y*vx.z*vz.w + vw.y*vx.w*vz.z;
  inv.vy.y = vx.x*vz.z*vw.w - vx.x*vz.w*vw.z - vz.x*vx.z*vw.w
           + vz.x*vx.w*vw.z + vw.x*vx.z*vz.w - vw.x*vx.w*vz.z;
  inv.vz.y = -vx.x*vz.y*vw.w + vx.x*vz.w*vw.y + vz.x*vx.y*vw.w
           - vz.x*vx.w*vw.y - vw.x*vx.y*vz.w + vw.x*vx.w*vz.y;
  inv.vw.y = vx.x*vz.y*vw.z - vx.x*vz.z*vw.y - vz.x*vx.y*vw.z
           + vz.x*vx.z*vw.y + vw.x*vx.y*vz.z - vw.x*vx.z*vz.y;
  inv.vx.z = vx.y*vy.z*vw.w - vx.y*vy.w*vw.z - vy.y*vx.z*vw.w
           + vy.y*vx.w*vw.z + vw.y*vx.z*vy.w - vw.y*vx.w*vy.z;
  inv.vy.z = -vx.x*vy.z*vw.w + vx.x*vy.w*vw.z + vy.x*vx.z*vw.w
           - vy.x*vx.w*vw.z - vw.x*vx.z*vy.w + vw.x*vx.w*vy.z;
  inv.vz.z = vx.x*vy.y*vw.w - vx.x*vy.w*vw.y - vy.x*vx.y*vw.w
           + vy.x*vx.w*vw.y + vw.x*vx.y*vy.w - vw.x*vx.w*vy.y;
  inv.vw.z = -vx.x*vy.y*vw.z + vx.x*vy.z*vw.y + vy.x*vx.y*vw.z
           - vy.x*vx.z*vw.y - vw.x*vx.y*vy.z + vw.x*vx.z*vy.y;
  inv.vx.w = -vx.y*vy.z*vz.w + vx.y*vy.w*vz.z + vy.y*vx.z*vz.w
           - vy.y*vx.w*vz.z - vz.y*vx.z*vy.w + vz.y*vx.w*vy.z;
  inv.vy.w = vx.x*vy.z*vz.w - vx.x*vy.w*vz.z - vy.x*vx.z*vz.w
           + vy.x*vx.w*vz.z + vz.x*vx.z*vy.w - vz.x*vx.w*vy.z;
  inv.vz.w = -vx.x*vy.y*vz.w + vx.x*vy.w*vz.y + vy.x*vx.y*vz.w
           - vy.x*vx.w*vz.y - vz.x*vx.y*vy.w + vz.x*vx.w*vy.y;
  inv.vw.w = vx.x*vy.y*vz.z - vx.x*vy.z*vz.y - vy.x*vx.y*vz.z
           + vy.x*vx.z*vz.y + vz.x*vx.y*vy.z - vz.x*vx.z*vy.y;
  return inv / (vx.x*inv.vx.x + vx.y*inv.vy.x + vx.z*inv.vz.x + vx.w*inv.vw.x);
}
template mat4x4<float> mat4x4<float>::inverse(void) const;
template mat4x4<double> mat4x4<double>::inverse(void) const;

const vec3i cubeiverts[8] = {
  vec3i(0,0,0)/*0*/, vec3i(0,0,1)/*1*/, vec3i(0,1,1)/*2*/, vec3i(0,1,0)/*3*/,
  vec3i(1,0,0)/*4*/, vec3i(1,0,1)/*5*/, vec3i(1,1,1)/*6*/, vec3i(1,1,0)/*7*/
};
const vec3f cubefverts[8] = {
  vec3f(0.f,0.f,0.f), vec3f(0.f,1.f,0.f), vec3f(0.f,1.f,1.f), vec3f(0.f,0.f,1.f),
  vec3f(1.f,0.f,0.f), vec3f(1.f,1.f,0.f), vec3f(1.f,1.f,1.f), vec3f(1.f,0.f,1.f)
};
const vec3i cubetris[12] = {
  vec3i(0,1,2), vec3i(0,2,3), vec3i(4,7,6), vec3i(4,6,5), // -x,+x triangles
  vec3i(0,4,5), vec3i(0,5,1), vec3i(2,6,7), vec3i(2,7,3), // -y,+y triangles
  vec3i(3,7,4), vec3i(3,4,0), vec3i(1,5,6), vec3i(1,6,2)  // -z,+z triangles
};
const vec3i cubenorms[6] = {
  vec3i(-1,0,0), vec3i(+1,0,0),
  vec3i(0,-1,0), vec3i(0,+1,0),
  vec3i(0,0,-1), vec3i(0,0,+1)
};
const vec2i cubeedges[12] = {
  vec2i(0,1),vec2i(1,2),vec2i(2,3),vec2i(3,0),
  vec2i(4,5),vec2i(5,6),vec2i(6,7),vec2i(7,4),
  vec2i(1,5),vec2i(2,6),vec2i(0,4),vec2i(3,7)
};
const vec4i cubequads[6] = {
  vec4i(0,1,2,3), vec4i(4,5,6,7),
  vec4i(0,4,7,3), vec4i(1,5,6,2),
  vec4i(0,4,5,1), vec4i(2,3,7,6)
};

camera::camera(vec3f org, vec3f up, vec3f view, float fov, float ratio) :
  org(org), up(up), view(view), fov(fov), ratio(ratio)
{
  const float left = -ratio * 0.5f;
  const float top = 0.5f;
  dist = 0.5f / tan(fov * float(pi) / 360.f);
  view = normalize(view);
  up = normalize(up);
  xaxis = cross(view, up);
  xaxis = normalize(xaxis);
  zaxis = cross(view, xaxis);
  zaxis = normalize(zaxis);
  imgplaneorg = dist*view + left*xaxis - top*zaxis;
  xaxis *= ratio;
}

} // namespace cube

