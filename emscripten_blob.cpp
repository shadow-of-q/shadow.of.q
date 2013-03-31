#include <emscripten.h>

#include "src/client.cpp"
#include "src/clientgame.cpp"
#include "src/command.cpp"
#include "src/console.cpp"
#include "src/editing.cpp"
#include "src/entities.cpp"
#include "src/ogl.cpp"
#include "src/main.cpp"
#include "src/menu.cpp"
#include "src/monster.cpp"
#include "src/physics.cpp"
#include "src/rendercubes.cpp"
#include "src/renderextras.cpp"
#include "src/rendermd2.cpp"
#include "src/renderparticles.cpp"
#include "src/rendertext.cpp"
#include "src/rndmap.cpp"
#include "src/demo.cpp"
#include "src/server.cpp"
#include "src/serverbrowser.cpp"
#include "src/serverms.cpp"
#include "src/serverutil.cpp"
#include "src/sound.cpp"
#include "src/tools.cpp"
#include "src/weapon.cpp"
#include "src/world.cpp"

#include "enet/callbacks.c"
#include "enet/host.c"
#include "enet/list.c"
#include "enet/memory.c"
#include "enet/packet.c"
#include "enet/peer.c"
#include "enet/protocol.c"
#include "enet/unix.c"

