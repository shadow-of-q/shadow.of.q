#pragma once
#include "command.hpp"
#include "console.hpp"
#include "menu.hpp"
#include "sound.hpp"
#include "client.hpp"
#include "clientgame.hpp"
#include "world.hpp"
#include "editing.hpp"
#include "entities.hpp"
#include "monster.hpp"
#include "physics.hpp"
#include "renderer.hpp"
#include "demo.hpp"
#include "serverbrowser.hpp"
#include "server.hpp"
#include "weapon.hpp"

#ifdef WIN32
  #define WIN32_LEAN_AND_MEAN
  #include "windows.h"
  #define _WINDOWS
  #define ZLIB_DLL
#endif

