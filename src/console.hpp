#pragma once
#include <cstdio>

// handle user-typed commands
namespace cube {
namespace console {

// handle a pressed key when console is up
void keypress(int code, bool isdown, int cooked);
// render buffer taking into account time & scrolling
void render(void);
// output a formatted string in the console
void out(const char *s, ...);
// get the string for the command currently typed
char *getcurcommand(void);
// write all the bindings in the given file
void writebinds(FILE *f);
// free all resources needed by the console
void clean(void);

} // namespace console
} // namespace cube

