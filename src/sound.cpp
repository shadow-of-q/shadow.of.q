#include "sound.hpp"
#include "cube.h"
#include "SDL/SDL_mixer.h"

#define MAXCHAN 32
#define SOUNDFREQ 22050
#define MAXVOL MIX_MAX_VOLUME

namespace cube {
namespace sound {

VARP(soundvol, 0, 255, 255);
VARP(musicvol, 0, 128, 255);
VAR(soundbufferlen, 128, 1024, 4096);
VAR(stereo, 0, 1, 1);

static bool nosound = false;
static Mix_Music *mod = NULL;
static void *stream = NULL;
static vector<Mix_Chunk *> samples;
static cvector snames;
static int soundsatonce = 0, lastsoundmillis = 0;
static struct { vec loc; bool inuse; } soundlocs[MAXCHAN];

void stop(void)
{
  if (nosound) return;
  if (mod) {
    Mix_HaltMusic();
    Mix_FreeMusic(mod);
    mod = NULL;
  }
  if (stream) stream = NULL;
}

void init(void)
{
  memset(soundlocs, 0, sizeof(soundlocs));
  if (Mix_OpenAudio(SOUNDFREQ, MIX_DEFAULT_FORMAT, 2, soundbufferlen) < 0) {
    console::out("sound init failed (SDL_mixer): %s", (size_t)Mix_GetError());
    nosound = true;
  }
  Mix_AllocateChannels(MAXCHAN);
}

static void music(const char *name)
{
  if (nosound) return;
  stop();
  if (soundvol && musicvol) {
    string sn;
    strcpy_s(sn, "packages/");
    strcat_s(sn, name);
    if ((mod = Mix_LoadMUS(path(sn)))) {
      Mix_PlayMusic(mod, -1);
      Mix_VolumeMusic((musicvol*MAXVOL)/255);
    }
  }
}

static int registersound(const char *name)
{
  loopv(snames) if (strcmp(snames[i], name)==0) return i;
  snames.add(newstring(name));
  samples.add(NULL);
  return samples.length()-1;
}

void clean(void)
{
  if (nosound) return;
  stop();
  Mix_CloseAudio();
}

static void updatechanvol(int chan, const vec *loc)
{
  int vol = soundvol, pan = 255/2;
  if (loc) {
    vdist(dist, v, *loc, player1->o);
    vol -= (int)(dist*3*soundvol/255);  // simple mono distance attenuation
    if (stereo && (v.x != 0 || v.y != 0)) {
      // relative angle of sound along X-Y axis
      const float yaw = -atan2(v.x, v.y) - player1->yaw*(PI / 180.0f);
      // range is from 0 (left) to 255 (right)
      pan = int(255.9f*(0.5*sin(yaw)+0.5f));
    }
  }
  vol = (vol*MAXVOL)/255;
  Mix_Volume(chan, vol);
  Mix_SetPanning(chan, 255-pan, pan);
}

static void newsoundloc(int chan, const vec *loc)
{
  assert(chan>=0 && chan<MAXCHAN);
  soundlocs[chan].loc = *loc;
  soundlocs[chan].inuse = true;
}

void updatevol(void)
{
  if (nosound) return;
  loopi(MAXCHAN) if (soundlocs[i].inuse) {
    if (Mix_Playing(i))
      updatechanvol(i, &soundlocs[i].loc);
    else
      soundlocs[i].inuse = false;
  }
}

void playc(int n)
{
  client::addmsg(0, 2, SV_SOUND, n);
  play(n);
}

void play(int n, const vec *loc)
{
  if (nosound) return;
  if (!soundvol) return;
  if (lastmillis==lastsoundmillis)
    soundsatonce++;
  else
    soundsatonce = 1;
  lastsoundmillis = lastmillis;
  if (soundsatonce>5) return;  // avoid bursts of sounds with heavy packetloss
  if (n<0 || n>=samples.length()) {
    console::out("unregistered sound: %d", n);
    return;
  }

  if (!samples[n]) {
    sprintf_sd(buf)("packages/sounds/%s.wav", snames[n]);
    samples[n] = Mix_LoadWAV(path(buf));
    if (!samples[n]) {
      console::out("failed to load sample: %s", buf);
      return;
    }
  }

  const int chan = Mix_PlayChannel(-1, samples[n], 0);
  if (chan<0) return;
  if (loc) newsoundloc(chan, loc);
  updatechanvol(chan, loc);
}

static void sound(int n) { play(n, NULL); }

COMMAND(music, ARG_1STR);
COMMAND(registersound, ARG_1EST);
COMMAND(sound, ARG_1INT);

} /* namespace sound */
} /* namespace cube */

