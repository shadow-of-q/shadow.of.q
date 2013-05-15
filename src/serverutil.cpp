#include "cube.hpp"
#include <enet/enet.h>

namespace cube {
namespace server {

  static string copyname;
  static int copysize;
  static uchar *copydata = NULL;

  void sendmaps(int n, string mapname, int mapsize, uchar *mapdata) {
    if (mapsize <= 0 || mapsize > 256*256) return;
    strcpy_s(copyname, mapname);
    copysize = mapsize;
    if (copydata) free(copydata);
    copydata = (uchar *)alloc(mapsize);
    memcpy(copydata, mapdata, mapsize);
  }

  ENetPacket *recvmap(int n) {
    if (!copydata) return NULL;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + copysize, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_RECVMAP);
    sendstring(copyname, p);
    putint(p, copysize);
    memcpy(p, copydata, copysize);
    p += copysize;
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    return packet;
  }

} // namespace server

#ifdef STANDALONE
void client::localservertoclient(uchar *buf, int len) {};
void fatal(const char *s, const char *o)
{
  server::cleanup();
  printf("servererror: %s\n", s);
  exit(1);
}
void *alloc(int s)
{
  void *b = calloc(1,s);
  if (!b) fatal("no memory!");
  return b;
}

int main(int argc, char* argv[])
{
  int uprate = 0, maxcl = 4;
  const char *sdesc = "", *ip = "", *master = NULL, *passwd = "";

  for (int i = 1; i<argc; i++) {
    char *a = &argv[i][2];
    if (argv[i][0]=='-') switch (argv[i][1]) {
      case 'u': uprate = atoi(a); break;
      case 'n': sdesc  = a; break;
      case 'i': ip     = a; break;
      case 'm': master = a; break;
      case 'p': passwd = a; break;
      case 'c': maxcl  = atoi(a); break;
      default: printf("WARNING: unknown commandline option\n");
    }
  }
  if (enet_initialize()<0) fatal("Unable to initialise network module");
  server::init(true, uprate, sdesc, ip, master, passwd, maxcl);
  return 0;
}
#endif

} // namespace cube

#ifdef STANDALONE
#include "game.cpp"
int main(int argc, char *argv[]) { return cube::main(argc,argv); }
#endif

