include_directories(../include)
include_directories(../enet/include)
set (CLIENT_SRC
  game.cpp
  bvh.cpp
  client.cpp
  command.cpp
  console.cpp
  editing.cpp
  entities.cpp
  ogl.cpp
  main.cpp
  math.cpp
  menu.cpp
  monster.cpp
  network.cpp
  physics.cpp
  renderer.cpp
  renderextras.cpp
  rendermd2.cpp
  renderparticles.cpp
  rendertext.cpp
  demo.cpp
  server.cpp
  serverbrowser.cpp
  serverms.cpp
  serverutil.cpp
  sound.cpp
  stl.cpp
  tools.cpp
  weapon.cpp
  world.cpp)

add_executable (cube_client ${CLIENT_SRC})
target_link_libraries (cube_client
  enet
  ${SDL_LIBRARY}
  ${SDL_MIXER_LIBRARY}
  ${SDL_IMAGE_LIBRARY}
  ${ZLIB_LIBRARY})

set (SERVER_SRC
  math.cpp
  network.cpp
  server.cpp
  serverms.cpp
  standalone.cpp
  stl.cpp
  tools.cpp)
add_executable (cube_server ${SERVER_SRC})
target_link_libraries (cube_server enet ${SDL_LIBRARY})
