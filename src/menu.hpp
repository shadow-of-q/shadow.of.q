#pragma once

// handle the stack of menus in the game
namespace cube {
namespace menu {

bool render(void);
void set(int menu);
void manual(int m, int n, char *text);
void sort(int start, int num);
bool key(int code, bool isdown);
void newm(const char *name);
void clean(void);

} // namespace menu
} // namespace cube

