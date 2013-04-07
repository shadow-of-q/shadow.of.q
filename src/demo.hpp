#ifndef __CUBE_DEMO_HPP__
#define __CUBE_DEMO_HPP__

#define SAVEGAMEVERSION 4 // bump if dynent/netprotocol changes or any other savegame/demo data

namespace cube {
struct vec;
namespace demo {

void loadgamerest(void);
void incomingdata(uchar *buf, int len, bool extras = false);
void playbackstep(void);
void stop(void);
void stopifrecording(void);
void damage(int damage, vec &o);
void blend(int damage);
bool playing(void);
int clientnum(void);

} // namespace demo
} // namespace cube

#endif // __CUBE_DEMO_HPP__

