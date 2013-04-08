#pragma once

namespace cube {
// XXX move that
extern bool editmode;
namespace edit {

void cursorupdate(void);
void toggleedit(void);
void editdrag(bool isdown);
bool noteditmode(void);
void pruneundos(int maxremain = 0);

} // namespace edit
} // namespace cube

