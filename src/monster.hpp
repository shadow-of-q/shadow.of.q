#ifndef __CUBE_MONSTER_HPP__
#define __CUBE_MONSTER_HPP__

namespace cube {
namespace monster {

enum { M_NONE, M_SEARCH, M_HOME, M_ATTACKING, M_PAIN, M_SLEEP, M_AIMING };
void monsterclear(void);
void restoremonsterstate(void);
void monsterthink(void);
void monsterrender(void);
dvector &getmonsters(void);
void monsterpain(dynent *m, int damage, dynent *d);
void endsp(bool allkilled);

} /* namespace monster */
} /* namespace cube */

#endif /* __CUBE_MONSTER_HPP__ */

