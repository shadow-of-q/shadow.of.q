CXX=g++
#CXX=clang++
#CXX=~/src/emscripten/em++
CXXOPTFLAGS=-Wall -O3 -g -fsigned-char -std=c++0x -fno-strict-aliasing
CXXFLAGS=$(CXXOPTFLAGS) -I../enet/include -I../include `sdl-config --cflags`

CLIENT_LIBS=-L../enet -lenet `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL -lX11
#CLIENT_OBJS=blob.o
CLIENT_OBJS= \
	game.o \
	bvh.o \
	client.o \
	command.o \
	console.o \
	editing.o \
	entities.o \
	ogl.o \
	main.o \
	math.o \
	menu.o \
	monster.o \
	network.o \
	physics.o \
	rendercubes.o \
	renderextras.o \
	rendermd2.o \
	renderparticles.o \
	rendertext.o \
	demo.o \
	server.o \
	serverbrowser.o \
	serverms.o \
	serverutil.o \
	sound.o \
	tools.o \
	weapon.o \
	world.o

SERVER_LIBS=-L../enet -lenet
SERVER_OBJS= \
	math.o \
	network.o \
	serverms.o \
	server.o \
	tools.o

default: all

all: enet client server

enet:
	-make -C ../enet all

clean:
	-rm -f $(SERVER_OBJS) $(CLIENT_OBJS) cube_server cube_client
	-make -C ../enet/ clean

standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -o standalone.o -c serverutil.cpp

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o cube_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o cube_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
