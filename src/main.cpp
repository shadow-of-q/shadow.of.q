#include "cube.hpp"
#include <SDL/SDL.h>
#include <enet/enet.h>
#include <time.h>

#if !defined(EMSCRIPTEN)
#include <xmmintrin.h>
#endif // EMSCRIPTEN

namespace cube {

void cleanup(char *msg) { // single program exit point;
  static int cleaningup = false; // avoid recursion
  if (!cleaningup) {
    cleaningup = true;
    demo::stop();
    client::disconnect(true);
    cmd::writecfg();
    game::clean();
    rr::clean();
    world::clean();
    sound::clean();
    server::clean();
    console::clean();
    menu::clean();
    cmd::clean();
    ogl::clean();
    SDL_ShowCursor(1);
    if (msg) {
#ifdef WIN32
      MessageBox(NULL, msg, "cube fatal error", MB_OK|MB_SYSTEMMODAL);
#else
      printf("%s",msg);
#endif
    }
  }
  SDL_Quit();
  exit(1);
}

void quit(void) { // normal exit
  browser::writeservercfg();
  cleanup(NULL);
}

void fatal(const char *s, const char *o) { // failure exit
  sprintf_sd(msg)("%s%s (%s)\n", s, o, SDL_GetError());
  cleanup(msg);
}

int scr_w = 800;
int scr_h = 600;

void screenshot(void) {
#if !defined(EMSCRIPTEN)
  SDL_Surface *image;
  SDL_Surface *temp;
  int idx;
  if ((image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)) != NULL) {
    if ((temp = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)) != NULL) {
      OGL (ReadPixels, 0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
      for (idx = 0; idx<scr_h; idx++) {
        char *dest = (char *)temp->pixels+3*scr_w*idx;
        memcpy(dest, (char *)image->pixels+3*scr_w*(scr_h-1-idx), 3*scr_w);
        endianswap(dest, 3, scr_w);
      }
      sprintf_sd(buf)("screenshots/screenshot_%d.bmp", game::lastmillis());
      SDL_SaveBMP(temp, path(buf));
      SDL_FreeSurface(temp);
    }
    SDL_FreeSurface(image);
  }
#endif // EMSCRIPTEN
}

COMMAND(screenshot, ARG_NONE);
COMMAND(quit, ARG_NONE);

void keyrepeat(bool on) {
  SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0, SDL_DEFAULT_REPEAT_INTERVAL);
}

VARF(gamespeed, 10, 100, 1000, if (client::multiplayer()) gamespeed = 100);
VARP(minmillis, 0, 5, 1000);
VARF(grabmouse, 0, 0, 1, {SDL_WM_GrabInput(grabmouse ? SDL_GRAB_ON : SDL_GRAB_OFF);});

int ignore = 5;

static void main_loop(void) {
  int millis = SDL_GetTicks()*gamespeed/100;
  if (millis-game::lastmillis()>200) game::setlastmillis(millis-200);
  else if (millis-game::lastmillis()<1) game::setlastmillis(millis-1);
#if !defined(EMSCRIPTEN)
  if (millis-game::lastmillis()<minmillis)
    SDL_Delay(minmillis-(millis-game::lastmillis()));
#endif
  game::updateworld(millis);
  if (!demo::playing())
    server::slice((int)time(NULL), 0);
  static float fps = 30.0f;
  fps = (1000.0f/game::curtime()+fps*50)/51;
  rr::readdepth(scr_w, scr_h);
  SDL_GL_SwapBuffers();
  sound::updatevol();
  ogl::drawframe(scr_w, scr_h, fps);
  SDL_Event event;
  int lasttype = 0, lastbut = 0;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        quit();
      break;
      case SDL_KEYDOWN: 
      case SDL_KEYUP: 
        console::keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode);
      break;
      case SDL_MOUSEMOTION:
        if (ignore) {
          ignore--;
          break;
        }
        game::mousemove(event.motion.xrel, event.motion.yrel);
      break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        if (lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
        console::keypress(-event.button.button, event.button.state!=0, 0);
        lasttype = event.type;
        lastbut = event.button.button;
      break;
    }
  }
}

static int installbasetex(int num, const char *name) {
  int xs, ys;
  char *str = path(NEWSTRING(name));
  const int ret = ogl::installtex(num, str, xs, ys);
  FREE(str);
  return ret;
}

static int main(int argc, char **argv) {
  IF_EMSCRIPTEN(emscripten_hide_mouse());
  bool dedicated = false;
  int fs = SDL_FULLSCREEN, par = 0, uprate = 0, maxcl = 4;
  const char *sdesc = "", *ip = "", *passwd = "";
  const char *master = NULL;

#if !defined(EMSCRIPTEN)
  // flush to zero and no denormals
  _mm_setcsr(_mm_getcsr() | (1<<15) | (1<<6));
#endif

  initendiancheck();

#define log(s) console::out("init: %s", s)
  log("sdl");

  for (int i = 1; i<argc; i++) {
    const char *a = &argv[i][2];
    if (argv[i][0]=='-') switch (argv[i][1]) {
      case 'd': dedicated = true; break;
      case 't': fs     = 0; break;
      case 'w': scr_w  = atoi(a); break;
      case 'h': scr_h  = atoi(a); break;
      case 'u': uprate = atoi(a); break;
      case 'n': sdesc  = a; break;
      case 'i': ip     = a; break;
      case 'm': master = a; break;
      case 'p': passwd = a; break;
      case 'c': maxcl  = atoi(a); break;
      default:  console::out("unknown commandline option");
    } else
      console::out("unknown commandline argument");
  }

#ifdef _DEBUG
  par = SDL_INIT_NOPARACHUTE;
  fs = 0;
#endif

  if (SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0)
    fatal("Unable to initialize SDL");

  log("memory subsystem");
  meminit();

  log("net");
  if (enet_initialize()<0)
    fatal("Unable to initialise network module");

  game::initclient();
  server::init(dedicated, uprate, sdesc, ip, master, passwd, maxcl);  // never returns if dedicated

  log("world");
  // world::empty(7, true);

  log("video: sdl");
  if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");
  SDL_WM_GrabInput(grabmouse ? SDL_GRAB_ON : SDL_GRAB_OFF);

  log("video: mode");
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  if (SDL_SetVideoMode(scr_w, scr_h, 0, SDL_OPENGL|fs)==NULL) fatal("Unable to create OpenGL screen");

  log("video: misc");
  SDL_WM_SetCaption("cube engine", NULL);
  keyrepeat(true);
  SDL_ShowCursor(0);

  log("gl");
  ogl::init(scr_w, scr_h);

  log("basetex");
  if (!installbasetex(1, "data/crosshair.png") ||
      !installbasetex(2, "data/newchars.png") ||
      !installbasetex(3, "data/martin/base.png") ||
      !installbasetex(5, "data/items.png") ||
      !installbasetex(4, "data/explosion.jpg") ||
      !installbasetex(6, "data/martin/ball1.png") ||
      !installbasetex(7, "data/martin/smoke.png") ||
      !installbasetex(8, "data/martin/ball2.png") ||
      !installbasetex(9, "data/martin/ball3.png"))
    fatal("could not find core textures (hint: run cube from the parent of the bin directory)");

  log("sound");
  sound::init();

  log("cfg");
  menu::newm("frags\tpj\tping\tteam\tname");
  menu::newm("ping\tplr\tserver");
  cmd::exec("data/keymap.cfg");
  cmd::exec("data/menus.cfg");
  cmd::exec("data/prefabs.cfg");
  cmd::exec("data/sounds.cfg");
  cmd::exec("servers.cfg");
  if (!cmd::execfile("config.cfg")) {
    console::out("config.cfg not found. executing defaults.cfg");
    cmd::execfile("data/defaults.cfg");
  }
  cmd::exec("autoexec.cfg");

  log("localconnect");
  server::localconnect();
  client::changemap("ee");

  log("mainloop");
#undef log

#if defined(EMSCRIPTEN)
  emscripten_set_main_loop(main_loop, 0, 1);
#else
  for(;;) main_loop();
#endif

  quit();
  return 1;
}

} // namespace cube

int main(int argc, char **argv) {return cube::main(argc, argv);}

