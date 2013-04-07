#ifndef __CUBE_EDITING_HPP__
#define __CUBE_EDITING_HPP__

namespace cube {

// XXX move that
extern bool editmode;

namespace edit {

void cursorupdate(void);
void toggleedit(void);
void editdrag(bool isdown);
bool noteditmode(void);
void pruneundos(int maxremain = 0);

} /* namespace edit */
} /* namespace cube */

#endif /* __CUBE_EDITING_HPP__ */

