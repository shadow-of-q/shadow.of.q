# SDL_FOUND
# SDL_INCLUDE_DIR
# SDL_LIBRARY

if (WIN32)
  find_path (SDL_INCLUDE_DIR SDL/SDL.h
             ${SDL_ROOT_PATH}/include
             ${PROJECT_SOURCE_DIR}/include
             doc "The directory where SDL.h and other SDL headers reside")
  find_library (SDL_LIBRARY
                NAMES SDL
                PATHS
                ${PROJECT_SOURCE_DIR}/lib
                DOC "The SDL library")
else (WIN32)
  find_path (SDL_INCLUDE_DIR SDL/SDL.h
             ~/include
             /usr/include
             /usr/local/include)
  find_library (SDL_LIBRARY SDL
                ~/lib
                /usr/lib
                /usr/local/lib)
endif (WIN32)

if (SDL_INCLUDE_DIR)
  set (SDL_FOUND 1 CACHE STRING "Set to 1 if SDL is found, 0 otherwise")
else (SDL_INCLUDE_DIR)
  set (SDL_FOUND 0 CACHE STRING "Set to 1 if SDL is found, 0 otherwise")
endif (SDL_INCLUDE_DIR)

mark_as_advanced (SDL_FOUND)

