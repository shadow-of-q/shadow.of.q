#include "renderer.hpp"

namespace cube {
namespace rr {
int curvert = 4;
int curmaxverts = 10000;
void clean(void) {
  mapmodelreset();
  cleanparticles();
}
} // namespace rr
} // namespace cube

