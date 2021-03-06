#include "cube.hpp"
#include <ctype.h>
#include <SDL/SDL.h>

#if !defined(__WIN32__)
#include <X11/Xlib.h>
#include <SDL/SDL_syswm.h>
#endif

namespace cube {
namespace console {

struct cline { char *cref; int outtime; };
static vector<cline> conlines;
static const int ndraw = 5;
static const unsigned int WORDWRAP = 80;
static int conskip = 0;
static bool saycommandon = false;
static string commandbuf;
static cvector vhistory;
static int histpos = 0;

static void setconskip(int n) {
  conskip += n;
  if (conskip < 0)
    conskip = 0;
}
COMMANDN(conskip, setconskip, ARG_1INT);

// keymap is defined externally in keymap.cfg
struct keym { int code; char *name; char *action; } keyms[256];
static int numkm = 0;

static void keymap(char *code, char *key, char *action) {
  keyms[numkm].code = atoi(code);
  keyms[numkm].name = NEWSTRING(key);
  keyms[numkm++].action = NEWSTRINGBUF(action);
}
COMMAND(keymap, ARG_3STR);

static void bindkey(char *key, char *action) {
  for (char *x = key; *x; x++) *x = toupper(*x);
  loopi(numkm) if (strcmp(keyms[i].name, key)==0) {
    strcpy_s(keyms[i].action, action);
    return;
  }
  out("unknown key \"%s\"", key);
}
COMMANDN(bind, bindkey, ARG_2STR);

void clean(void) {
  loopv(conlines) FREE(conlines[i].cref);
  loopi(numkm) {
    FREE(keyms[i].name);
    FREE(keyms[i].action);
  }
}

static void line(const char *sf, bool highlight) {
  cline cl;
  if (conlines.size()>100) {
    cl.cref = conlines.back().cref;
    conlines.pop_back();
  } else
    cl.cref = NEWSTRINGBUF("");
  cl.outtime = game::lastmillis(); // for how long to keep line on screen
  conlines.insert(conlines.begin(),cl);
  if (highlight) { // show line in a different colour, for chat etc.
    cl.cref[0] = '\f';
    cl.cref[1] = 0;
    strcat_s(cl.cref, sf);
  } else
    strcpy_s(cl.cref, sf);
  puts(cl.cref);
#if defined(__WIN32__)
  fflush(stdout);
#endif
}

void out(const char *s, ...) {
  sprintf_sdv(sf, s);
  s = sf;
  int n = 0;
  while (strlen(s)>WORDWRAP) { // cut strings to fit on screen
    string t;
    strn0cpy(t, s, WORDWRAP+1);
    line(t, n++!=0);
    s += WORDWRAP;
  }
  line(s, n!=0);
}

void render(void) {
  int nd = 0;
  char *refs[ndraw];
  loopv(conlines)
    if (conskip ? i>=conskip-1 || i>=conlines.size()-ndraw :
       game::lastmillis()-conlines[i].outtime<20000) {
      refs[nd++] = conlines[i].cref;
      if (nd==ndraw) break;
    }
  const int h = rr::FONTH;
  loopj(nd) rr::drawtext(refs[j], h/3, (h/4*5)*(nd-j-1)+h/3, 2);
}

// turns input to the command line on or off
static void saycommand(const char *init) {
  SDL_EnableUNICODE(saycommandon = (init!=NULL));
  if (!edit::mode()) keyrepeat(saycommandon);
  if (!init) init = "";
  strcpy_s(commandbuf, init);
}
COMMAND(saycommand, ARG_VARI);

static void mapmsg(char *s) { strn0cpy(world::maptitle(), s, 128); }
COMMAND(mapmsg, ARG_1STR);

static void history(int n) {
  static bool rec = false;
  if (!rec && n>=0 && n<vhistory.size()) {
    rec = true;
    cmd::execute(vhistory[vhistory.size()-n-1]);
    rec = false;
  }
}
COMMAND(history, ARG_1INT);

void keypress(int code, bool isdown, int cooked) {
  if (saycommandon) { // keystrokes go to commandline
    if (isdown) {
      switch (code) {
        case SDLK_RETURN:
          break;
        case SDLK_BACKSPACE:
        case SDLK_LEFT:
        {
          for (int i = 0; commandbuf[i]; i++) if (!commandbuf[i+1]) commandbuf[i] = 0;
          cmd::resetcomplete();
          break;
        }
        case SDLK_UP:
          if (histpos) strcpy_s(commandbuf, vhistory[--histpos]);
          break;
        case SDLK_DOWN:
          if (histpos<vhistory.size()) strcpy_s(commandbuf, vhistory[histpos++]);
          break;
        case SDLK_TAB:
          cmd::complete(commandbuf);
          break;
        default:
          cmd::resetcomplete();
          if (cooked) {
            const char add[] = { char(cooked), 0 };
            strcat_s(commandbuf, add);
          }
      }
    } else {
      if (code==SDLK_RETURN) {
        if (commandbuf[0]) {
          if (vhistory.empty() || strcmp(vhistory.back(), commandbuf))
            vhistory.add(NEWSTRING(commandbuf));  // cap this?
          histpos = vhistory.size();
          if (commandbuf[0]=='/')
            cmd::execute(commandbuf, true);
          else
            client::toserver(commandbuf);
        }
        saycommand(NULL);
      }
      else if (code==SDLK_ESCAPE)
        saycommand(NULL);
    }
  } else if (!menu::key(code, isdown)) { // keystrokes go to menu
    loopi(numkm) if (keyms[i].code==code) { // keystrokes go to game, lookup in keymap and execute
      string temp;
      strcpy_s(temp, keyms[i].action);
      cmd::execute(temp, isdown);
      return;
    }
  }
}

char *getcurcommand() { return saycommandon ? commandbuf : NULL; }

void writebinds(FILE *f) {
  loopi(numkm)
    if (*keyms[i].action)
      fprintf(f, "bind \"%s\" [%s]\n", keyms[i].name, keyms[i].action);
}

} // namespace console
} // namespace cube

