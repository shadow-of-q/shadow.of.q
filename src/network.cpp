#include "tools.hpp"

// all network traffic is in 32bit ints, which are then compressed using the
// following simple scheme (assumes that most values are small).
namespace cube {
namespace server {

  void putint(uchar *&p, int n) {
    if (n<128 && n>-127) { *p++ = n; }
    else if (n<0x8000 && n>=-0x8000) { *p++ = 0x80; *p++ = n; *p++ = n>>8;  }
    else { *p++ = 0x81; *p++ = n; *p++ = n>>8; *p++ = n>>16; *p++ = n>>24; };
  }

  int getint(uchar *&p) {
    int c = *((char *)p);
    p++;
    if (c==-128) { int n = *p++; n |= *((char *)p)<<8; p++; return n;}
    else if (c==-127) { int n = *p++; n |= *p++<<8; n |= *p++<<16; return n|(*p++<<24); } 
    else return c;
  }

  void sendstring(const char *t, uchar *&p) {
    while (*t) putint(p, *t++);
    putint(p, 0);
  }

} // namespace server
} // namespace cube

