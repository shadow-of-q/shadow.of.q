include_directories(../include)
include_directories(../enet/include)
set (CLIENT_SRC
  base/command.cpp
  base/stl.cpp
  base/task.cpp
  base/tools.cpp
  base/math.cpp
  game.cpp
  bvh.cpp
  client.cpp
  console.cpp
  editing.cpp
  entities.cpp
  main.cpp
  menu.cpp
  monster.cpp
  network.cpp
  obj.cpp
  ogl.cpp
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
  base/math.cpp
  base/stl.cpp
  base/tools.cpp
  network.cpp
  server.cpp
  serverms.cpp
  standalone.cpp)
add_executable (cube_server ${SERVER_SRC})
target_link_libraries (cube_server enet ${SDL_LIBRARY})

if (TEST_TASKS)
  set (TEST_TASK_SRC
    base/math.cpp
    base/stl.cpp
    base/tools.cpp
    base/task.cpp
    utests/task.cpp)
  add_executable (testtask ${TEST_TASK_SRC})
  target_link_libraries (testtask ${SDL_LIBRARY})
endif (TEST_TASKS)

