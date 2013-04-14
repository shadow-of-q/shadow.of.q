#pragma once

namespace cube {
namespace edit {

bool mode(void);
void cursorupdate(void);
void toggleedit(void);
void editdrag(bool isdown);
bool noteditmode(void);
void pruneundos(int maxremain = 0);

} // namespace edit
} // namespace cube

