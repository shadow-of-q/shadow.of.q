#pragma once
#define SAVEGAMEVERSION 4 // bump if dynent/netprotocol changes or any other savegame/demo data

#include "tools.hpp"

namespace cube {
namespace demo {

void loadgamerest(void);
void incomingdata(uchar *buf, int len, bool extras = false);
void playbackstep(void);
void stop(void);
void stopifrecording(void);
void damage(int damage, vec3f &o);
void blend(int damage);
bool playing(void);
int clientnum(void);

} // namespace demo
} // namespace cube

