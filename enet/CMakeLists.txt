set (ENET_SRC
  callbacks.c
  host.c
  list.c
  memory.c
  packet.c
  peer.c
  protocol.c
  unix.c)
include_directories (include)
add_definitions (-DHAS_FCNTL -DHAS_SOCKLEN_T -DHAS_POLL)
add_library (enet STATIC ${ENET_SRC})

