# SDL_IMAGE_FOUND
# SDL_IMAGE_INCLUDE_DIR
# SDL_IMAGE_LIBRARY

if (WIN32)
  find_path (SDL_IMAGE_INCLUDE_DIR SDL/SDL_image.h
             ${SDL_ROOT_PATH}/include
             ${PROJECT_SOURCE_DIR}/include
             doc "The directory where SDL_image.h header resides")
  find_library (SDL_IMAGE_LIBRARY
                NAMES SDL_image
                PATHS
                ${PROJECT_SOURCE_DIR}/lib
                DOC "The SDL_image library")
else (WIN32)
  find_path (SDL_IMAGE_INCLUDE_DIR SDL/SDL_image.h
             ~/include
             /usr/include
             /usr/local/include)
  find_library (SDL_IMAGE_LIBRARY SDL_image
                ~/lib
                /usr/lib
                /usr/local/lib)
endif (WIN32)

if (SDL_IMAGE_INCLUDE_DIR)
  set (SDL_IMAGE_FOUND 1 CACHE STRING "Set to 1 if SDL_image is found, 0 otherwise")
else (SDL_IMAGE_INCLUDE_DIR)
  set (SDL_IMAGE_FOUND 0 CACHE STRING "Set to 1 if SDL_image is found, 0 otherwise")
endif (SDL_IMAGE_INCLUDE_DIR)

mark_as_advanced (SDL_IMAGE_FOUND)

