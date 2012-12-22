#include "cube.h"
#include <GL/gl.h>
#include <SDL/SDL.h>
#include <enet/enet.h>
#include <time.h>

void cleanup(char *msg)         // single program exit point;
{
  demo::stop();
  client::disconnect(true);
  cmd::writecfg();
  renderer::cleangl();
  sound::clean();
  server::cleanup();
  SDL_ShowCursor(1);
  if (msg)
  {
#ifdef WIN32
    MessageBox(NULL, msg, "cube fatal error", MB_OK|MB_SYSTEMMODAL);
#else
    printf(msg);
#endif
  };
  SDL_Quit();
  exit(1);
};

void quit() // normal exit
{
  browser::writeservercfg();
  cleanup(NULL);
};

void fatal(const char *s, const char *o)    // failure exit
{
  sprintf_sd(msg)("%s%s (%s)\n", s, o, SDL_GetError());
  cleanup(msg);
};

void *alloc(int s) // for some big chunks... most other allocs use the memory pool
{
  void *b = calloc(1,s);
  if (!b) fatal("out of memory!");
  return b;
};

int scr_w = 1920;
int scr_h = 1080;

void screenshot()
{
  SDL_Surface *image;
  SDL_Surface *temp;
  int idx;
  if ((image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)) != NULL)
  {
    if ((temp = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)) != NULL)
    {
      glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
      for (idx = 0; idx<scr_h; idx++)
      {
        char *dest = (char *)temp->pixels+3*scr_w*idx;
        memcpy(dest, (char *)image->pixels+3*scr_w*(scr_h-1-idx), 3*scr_w);
        endianswap(dest, 3, scr_w);
      };
      sprintf_sd(buf)("screenshots/screenshot_%d.bmp", lastmillis);
      SDL_SaveBMP(temp, path(buf));
      SDL_FreeSurface(temp);
    };
    SDL_FreeSurface(image);
  };
};

COMMAND(screenshot, ARG_NONE);
COMMAND(quit, ARG_NONE);

void keyrepeat(bool on)
{
  SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
      SDL_DEFAULT_REPEAT_INTERVAL);
};

VARF(gamespeed, 10, 100, 1000, if (client::multiplayer()) gamespeed = 100);
VARP(minmillis, 0, 5, 1000);
VARF(grabmouse, 0, 0, 1, {SDL_WM_GrabInput(grabmouse ? SDL_GRAB_ON : SDL_GRAB_OFF);});

int islittleendian = 1;
int framesinmap = 0;

int main(int argc, char **argv)
{    
  bool dedicated = false;
  int fs = SDL_FULLSCREEN, par = 0, uprate = 0, maxcl = 4;
  const char *sdesc = "", *ip = "", *passwd = "";
  const char *master = NULL;
  islittleendian = *((char *)&islittleendian);

#define log(s) console::out("init: %s", s)
  log("sdl");

  for (int i = 1; i<argc; i++)
  {
    const char *a = &argv[i][2];
    if (argv[i][0]=='-') switch (argv[i][1])
    {
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
    }
    else console::out("unknown commandline argument");
  };

#ifdef _DEBUG
  par = SDL_INIT_NOPARACHUTE;
  fs = 0;
#endif

  if (SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");

  log("net");
  if (enet_initialize()<0) fatal("Unable to initialise network module");

  game::initclient();
  server::init(dedicated, uprate, sdesc, ip, master, passwd, maxcl);  // never returns if dedicated

  log("world");
  world::empty(7, true);

  log("video: sdl");
  if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

  log("video: mode");
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  if (SDL_SetVideoMode(scr_w, scr_h, 0, SDL_OPENGL|fs)==NULL) fatal("Unable to create OpenGL screen");

  log("video: misc");
  SDL_WM_SetCaption("cube engine", NULL);
  SDL_WM_GrabInput(SDL_GRAB_ON);
  keyrepeat(true);
  SDL_ShowCursor(0);

  log("gl");
  renderer::gl_init(scr_w, scr_h);
  SDL_WM_GrabInput(grabmouse ? SDL_GRAB_ON : SDL_GRAB_OFF);

  log("basetex");
  int xs, ys;
  if (!renderer::installtex(2,  path(newstring("data/newchars.png")), xs, ys) ||
      !renderer::installtex(3,  path(newstring("data/martin/base.png")), xs, ys) ||
      !renderer::installtex(6,  path(newstring("data/martin/ball1.png")), xs, ys) ||
      !renderer::installtex(7,  path(newstring("data/martin/smoke.png")), xs, ys) ||
      !renderer::installtex(8,  path(newstring("data/martin/ball2.png")), xs, ys) ||
      !renderer::installtex(9,  path(newstring("data/martin/ball3.png")), xs, ys) ||
      !renderer::installtex(4,  path(newstring("data/explosion.jpg")), xs, ys) ||
      !renderer::installtex(5,  path(newstring("data/items.png")), xs, ys) ||
      !renderer::installtex(1,  path(newstring("data/crosshair.png")), xs, ys))
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
  if (!cmd::execfile("config.cfg")) cmd::execfile("data/defaults.cfg");
  cmd::exec("autoexec.cfg");

  log("localconnect");
  server::localconnect();
  client::changemap("metl3");        // if this map is changed, also change depthcorrect()

  log("mainloop");
  int ignore = 5;
  for (;;)
  {
    int millis = SDL_GetTicks()*gamespeed/100;
    if (millis-lastmillis>200) lastmillis = millis-200;
    else if (millis-lastmillis<1) lastmillis = millis-1;
    if (millis-lastmillis<minmillis)
      SDL_Delay(minmillis-(millis-lastmillis));
    world::cleardlights();
    game::updateworld(millis);
    if (!demoplayback)
      server::slice((int)time(NULL), 0);
    static float fps = 30.0f;
    fps = (1000.0f/curtime+fps*50)/51;
    world::computeraytable(player1->o.x, player1->o.y);
    renderer::readdepth(scr_w, scr_h);
    SDL_GL_SwapBuffers();
    sound::updatevol();
    if (framesinmap++<5)    // cheap hack to get rid of initial sparklies, even when triple buffering etc.
    {
      player1->yaw += 5;
      renderer::gl_drawframe(scr_w, scr_h, fps);
      player1->yaw -= 5;
    }
    renderer::gl_drawframe(scr_w, scr_h, fps);
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
          if (ignore) { ignore--; break; };
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
  quit();
  return 1;
}

