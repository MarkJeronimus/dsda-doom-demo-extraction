/* Emacs style mode select   -*- C -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  System interface for sound.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_mutex.h"

#include "SDL_endian.h"

#include "SDL_version.h"
#include "SDL_thread.h"
#define USE_RWOPS
#include "SDL_mixer.h"

#include "z_zone.h"

#include "m_swap.h"
#include "i_sound.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "d_main.h"
#include "i_system.h"

//e6y
#include "e6y.h"

#include "dsda/settings.h"

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.

// The actual output device.
int audio_fd;

typedef struct
{
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  int id;
  // The channel step amount...
  unsigned int step;
  // ... and a 0.16 bit remainder of last step.
  unsigned int stepremainder;
  unsigned int samplerate;
  unsigned int bits;
  // The channel data pointers, start and end.
  const unsigned char *data;
  const unsigned char *startdata;
  const unsigned char *enddata;
  // Time/gametic that the channel started playing,
  //  used to determine oldest, which automatically
  //  has lowest priority.
  // In case number of active sounds exceeds
  //  available channels.
  int starttime;
  // left and right channel volume (0-127)
  int leftvol;
  int rightvol;
  dboolean loop;
} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS];

// Pitch to stepping lookup, unused.
int   steptable[256];

// Volume lookups.
//int   vol_lookup[128 * 256];

// NSM
static int dumping_sound = 0;


// lock for updating any params related to sfx
SDL_mutex *sfxmutex;

static int pitched_sounds;
int snd_samplerate; // samples per second
static int snd_samplecount;

void I_InitSoundParams(void)
{
  pitched_sounds = dsda_IntConfig(dsda_config_pitched_sounds);

  // TODO: can we reinitialize sound with new sample rate / count?
  if (!snd_samplerate)
    snd_samplerate = dsda_IntConfig(dsda_config_snd_samplerate);
  if (!snd_samplecount)
    snd_samplecount = dsda_IntConfig(dsda_config_snd_samplecount);
}

/* cph
 * stopchan
 * Stops a sound, unlocks the data
 */

static void stopchan(int i)
{
  if (channelinfo[i].data) /* cph - prevent excess unlocks */
  {
    channelinfo[i].data = NULL;
  }
}

typedef struct wav_data_s
{
  int sfxid;
  const unsigned char *data;
  int samplelen;
  int samplerate;
  int bits;
  struct wav_data_s *next;
} wav_data_t;

#define WAV_DATA_HASH_SIZE 32
static wav_data_t *wav_data_hash[WAV_DATA_HASH_SIZE];

static wav_data_t *GetWavData(int sfxid, const unsigned char *data, size_t len)
{
  int key;
  wav_data_t *target = NULL;

  key = (sfxid % WAV_DATA_HASH_SIZE);

  if (wav_data_hash[key])
  {
    wav_data_t *rover = wav_data_hash[key];

    while (rover)
    {
      if (rover->sfxid == sfxid)
      {
        target = rover;
        break;
      }

      rover = rover->next;
    }
  }

  if (target == NULL &&
      len > 44 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WAVEfmt ", 8))
  {
    SDL_RWops *RWops;
    SDL_AudioSpec wav_spec;
    Uint8 *wav_buffer = NULL;
    int bits, samplelen;

    RWops = SDL_RWFromConstMem(data, len);

    if (SDL_LoadWAV_RW(RWops, 1, &wav_spec, &wav_buffer, &samplelen) == NULL)
    {
      lprintf(LO_WARN, "Could not open wav file: %s\n", SDL_GetError());
      return NULL;
    }

    if (wav_spec.channels != 1)
    {
      lprintf(LO_WARN, "Only mono WAV file is supported");
      SDL_FreeWAV(wav_buffer);
      return NULL;
    }

    if (!SDL_AUDIO_ISINT(wav_spec.format))
    {
      lprintf(LO_WARN, "WAV file in unsupported format");
      SDL_FreeWAV(wav_buffer);
      return NULL;
    }

    bits = SDL_AUDIO_BITSIZE(wav_spec.format);
    if (bits != 8 && bits != 16)
    {
      lprintf(LO_WARN, "Only 8 or 16 bit WAV files are supported");
      SDL_FreeWAV(wav_buffer);
      return NULL;
    }

    target = Z_Malloc(sizeof(*target));

    target->sfxid = sfxid;
    target->data = wav_buffer;
    target->samplelen = samplelen;
    target->samplerate = wav_spec.freq;
    target->bits = bits;

    // use head insertion
    target->next = wav_data_hash[key];
    wav_data_hash[key] = target;
  }

  return target;
}

//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
static int addsfx(int sfxid, int channel, const unsigned char *data, size_t len)
{
  channel_info_t *ci = channelinfo + channel;
  wav_data_t *wav_data = GetWavData(sfxid, data, len);

  stopchan(channel);

  if (wav_data)
  {
    ci->data = wav_data->data;
    ci->enddata = ci->data + wav_data->samplelen - 1;
    ci->samplerate = wav_data->samplerate;
    ci->bits = wav_data->bits;
  }
  else
  {
    ci->data = data;
    /* Set pointer to end of raw data. */
    ci->enddata = ci->data + len - 1;
    ci->samplerate = (ci->data[3] << 8) + ci->data[2];
    ci->data += 8; /* Skip header */
    ci->bits = 8;
  }

  ci->stepremainder = 0;
  // Should be gametic, I presume.
  ci->starttime = gametic;

  ci->startdata = ci->data;

  // Preserve sound SFX id,
  //  e.g. for avoiding duplicates of chainsaw.
  ci->id = sfxid;

  return channel;
}

static int getSliceSize(void)
{
  int limit, n;

  if (snd_samplecount >= 32)
    return snd_samplecount * snd_samplerate / 11025;

  limit = snd_samplerate / TICRATE;

  // Try all powers of two, not exceeding the limit.

  for (n = 0; ; ++n)
  {
    // 2^n <= limit < 2^n+1 ?

    if ((1 << (n + 1)) > limit)
    {
      return (1 << n);
    }
  }

  // Should never happen?

  return 1024;
}

static void updateSoundParams(int handle, sfx_params_t *params)
{
  int slot = handle;
  int rightvol;
  int leftvol;
  int step = steptable[params->pitch];

#ifdef RANGECHECK
  if ((handle < 0) || (handle >= MAX_CHANNELS))
    I_Error("I_UpdateSoundParams: handle out of range");
#endif

  channelinfo[slot].loop = params->loop;

  // Set stepping
  // MWM 2000-12-24: Calculates proportion of channel samplerate
  // to global samplerate for mixing purposes.
  // Patched to shift left *then* divide, to minimize roundoff errors
  // as well as to use SAMPLERATE as defined above, not to assume 11025 Hz
  if (pitched_sounds)
    channelinfo[slot].step = step + (((channelinfo[slot].samplerate << 16) / snd_samplerate) - 65536);
  else
    channelinfo[slot].step = ((channelinfo[slot].samplerate << 16) / snd_samplerate);

  // Separation, that is, orientation/stereo.
  //  range is: 1 - 256
  params->separation += 1;

  // Per left/right channel.
  //  x^2 separation,
  //  adjust volume properly.
  leftvol = params->volume - ((params->volume * params->separation * params->separation) >> 16);
  params->separation = params->separation - 257;
  rightvol = params->volume - ((params->volume * params->separation * params->separation) >> 16);

  // Sanity check, clamp volume.
  if (rightvol < 0 || rightvol > 127)
  {
    rightvol = rightvol < 0 ? 0 : 127;
    lprintf(LO_WARN, "rightvol out of bounds\n");
  }

  if (leftvol < 0 || leftvol > 127)
  {
    leftvol = leftvol < 0 ? 0 : 127;
    lprintf(LO_WARN, "leftvol out of bounds\n");
  }

  // Get the proper lookup table piece
  //  for this volume level???
  channelinfo[slot].leftvol = leftvol;
  channelinfo[slot].rightvol = rightvol;
}

void I_UpdateSoundParams(int handle, sfx_params_t *params)
{
  SDL_LockMutex (sfxmutex);
  updateSoundParams(handle, params);
  SDL_UnlockMutex (sfxmutex);
}

//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels(void)
{
  // Init internal lookups (raw data, mixing buffer, channels).
  // This function sets up internal lookups used during
  //  the mixing process.
  int   i;
  //int   j;

  int  *steptablemid = steptable + 128;

  // Okay, reset internal mixing channels to zero.
  for (i = 0; i < MAX_CHANNELS; i++)
  {
    memset(&channelinfo[i], 0, sizeof(channel_info_t));
  }

  // This table provides step widths for pitch parameters.
  // I fail to see that this is currently used.
  for (i = -128 ; i < 128 ; i++)
    steptablemid[i] = (int)(pow(1.2, ((double)i / (64.0 * snd_samplerate / 11025))) * 65536.0);


  // Generates volume lookup tables
  //  which also turn the unsigned samples
  //  into signed samples.
  /*
  for (i = 0 ; i < 128 ; i++)
    for (j = 0 ; j < 256 ; j++)
    {
      // proff - made this a little bit softer, because with
      // full volume the sound clipped badly
      vol_lookup[i * 256 + j] = (i * (j - 128) * 256) / 191;
      //vol_lookup[i*256+j] = (i*(j-128)*256)/127;
    }
  */
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
  if (sfx->link)
    sfx = sfx->link;

  if (!sfx->name)
    return LUMP_NOT_FOUND;

  return W_CheckNumForName(sfx->name); //e6y: make missing sounds non-fatal
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int I_StartSound(int id, int channel, sfx_params_t *params)
{
  const unsigned char *data;
  int lump;
  size_t len;

  if ((channel < 0) || (channel >= MAX_CHANNELS))
#ifdef RANGECHECK
    I_Error("I_StartSound: handle out of range");
#else
    return -1;
#endif

  lump = S_sfx[id].lumpnum;

  // We will handle the new SFX.
  // Set pointer to raw data.
  len = W_LumpLength(lump);

  // e6y: Crash with zero-length sounds.
  // Example wad: dakills (http://www.doomworld.com/idgames/index.php?id=2803)
  // The entries DSBSPWLK, DSBSPACT, DSSWTCHN and DSSWTCHX are all zero-length sounds
  if (len <= 8) return -1;

  /* Find padded length */
  len -= 8;
  // do the lump caching outside the SDL_LockAudio/SDL_UnlockAudio pair
  // use locking which makes sure the sound data is in a malloced area and
  // not in a memory mapped one
  data = (const unsigned char *)W_LockLumpNum(lump);

  SDL_LockMutex (sfxmutex);

  // Returns a handle (not used).
  addsfx(id, channel, data, len);
  updateSoundParams(channel, params);

  SDL_UnlockMutex (sfxmutex);


  return channel;
}



void I_StopSound (int handle)
{
#ifdef RANGECHECK
  if ((handle < 0) || (handle >= MAX_CHANNELS))
    I_Error("I_StopSound: handle out of range");
#endif

  SDL_LockMutex (sfxmutex);
  stopchan(handle);
  SDL_UnlockMutex (sfxmutex);
}


dboolean I_SoundIsPlaying(int handle)
{
#ifdef RANGECHECK
  if ((handle < 0) || (handle >= MAX_CHANNELS))
    I_Error("I_SoundIsPlaying: handle out of range");
#endif

  return channelinfo[handle].data != NULL;
}


dboolean I_AnySoundStillPlaying(void)
{
  dboolean result = false;
  int i;

  for (i = 0; i < MAX_CHANNELS; i++)
    result |= channelinfo[i].data != NULL;

  return result;
}


//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the given
//  mixing buffer, and clamping it to the allowed
//  range.
//
// This function currently supports only 16bit.
//

static void I_UpdateSound(void *unused, Uint8 *stream, int len)
{
  // Mix current sound data.
  // Data, from raw sound, for right and left.
  // register unsigned char sample;
  register int    dl;
  register int    dr;

  // Pointers in audio stream, left, right, end.
  signed short   *leftout;
  signed short   *rightout;
  signed short   *leftend;
  // Step in stream, left and right, thus two.
  int       step;

  // Mixing channel index.
  int       chan;

  memset(stream, 0, len);

  // NSM: when dumping sound, ignore the callback calls and only
  // service dumping calls
  if (dumping_sound && unused != (void *) 0xdeadbeef)
    return;

  SDL_LockMutex (sfxmutex);
  // Left and right channel
  //  are in audio stream, alternating.
  leftout = (signed short *)stream;
  rightout = ((signed short *)stream) + 1;
  step = 2;

  // Determine end, for left channel only
  //  (right channel is implicit).
  leftend = leftout + (len / 4) * step;

  // Mix sounds into the mixing buffer.
  // Loop over step*SAMPLECOUNT,
  //  that is 512 values for two channels.
  while (leftout != leftend)
  {
    // Reset left/right value.
    //dl = 0;
    //dr = 0;
    dl = *leftout;
    dr = *rightout;

    // Love thy L2 chache - made this a loop.
    // Now more channels could be set at compile time
    //  as well. Thus loop those  channels.
    for ( chan = 0; chan < numChannels; chan++ )
    {
      channel_info_t *ci = channelinfo + chan;

      // Check channel, if active.
      if (ci->data)
      {
        int s;
        // Get the raw data from the channel.
        // no filtering
        //int s = channelinfo[chan].data[0] * 0x10000 - 0x800000;

        // linear filtering
        // the old SRC did linear interpolation back into 8 bit, and then expanded to 16 bit.
        // this does interpolation and 8->16 at same time, allowing slightly higher quality
        if (ci->bits == 16)
        {
          s = (short)(ci->data[0] | (ci->data[1] << 8)) * (255 - (ci->stepremainder >> 8))
            + (short)(ci->data[2] | (ci->data[3] << 8)) * (ci->stepremainder >> 8);
        }
        else
        {
          s = ((unsigned int)ci->data[0] * (0x10000 - ci->stepremainder))
            + ((unsigned int)ci->data[1] * (ci->stepremainder))
            - 0x800000; // convert to signed
        }


        // Add left and right part
        //  for this channel (sound)
        //  to the current data.
        // Adjust volume accordingly.

        // full loudness (vol=127) is actually 127/191

        dl += ci->leftvol * s / 49152;  // >> 15;
        dr += ci->rightvol * s / 49152; // >> 15;

        // Increment index ???
        ci->stepremainder += ci->step;

        // MSB is next sample???
        if (ci->bits == 16)
          ci->data += (ci->stepremainder >> 16) * 2;
        else
          ci->data += ci->stepremainder >> 16;

        // Limit to LSB???
        ci->stepremainder &= 0xffff;

        // Check whether we are done.
        if (ci->data >= ci->enddata)
        {
          if (ci->loop)
            ci->data = ci->startdata;
          else
            stopchan(chan);
        }
      }
    }

    // Clamp to range. Left hardware channel.
    // Has been char instead of short.
    // if (dl > 127) *leftout = 127;
    // else if (dl < -128) *leftout = -128;
    // else *leftout = dl;

    if (dl > SHRT_MAX)
      *leftout = SHRT_MAX;
    else if (dl < SHRT_MIN)
      *leftout = SHRT_MIN;
    else
      *leftout = (signed short)dl;

    // Same for right hardware channel.
    if (dr > SHRT_MAX)
      *rightout = SHRT_MAX;
    else if (dr < SHRT_MIN)
      *rightout = SHRT_MIN;
    else
      *rightout = (signed short)dr;

    // Increment current pointers in stream
    leftout += step;
    rightout += step;
  }
  SDL_UnlockMutex (sfxmutex);
}

static dboolean sound_was_initialized;

void I_ShutdownSound(void)
{
  if (sound_was_initialized)
  {
    Mix_CloseAudio();
    SDL_CloseAudio();

    sound_was_initialized = false;

    if (sfxmutex)
    {
      SDL_DestroyMutex (sfxmutex);
      sfxmutex = NULL;
    }
  }
}

void I_InitSound(void)
{
  int audio_rate;
  int audio_channels;
  int audio_buffers;

  if (sound_was_initialized || nosfxparm)
    return;

  if (SDL_InitSubSystem(SDL_INIT_AUDIO))
  {
    lprintf(LO_WARN, "Couldn't initialize SDL audio (%s))\n", SDL_GetError());
    nosfxparm = true;
    return;
  }

  // Secure and configure sound device first.
  lprintf(LO_DEBUG, "I_InitSound: ");

  I_InitSoundParams();

  audio_rate = snd_samplerate;
  audio_channels = 2;
  audio_buffers = getSliceSize();

  if (Mix_OpenAudioDevice(audio_rate, MIX_DEFAULT_FORMAT, audio_channels, audio_buffers,
                          NULL, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) < 0)
  {
    lprintf(LO_DEBUG, "couldn't open audio with desired format (%s)\n", SDL_GetError());
    nosfxparm = true;
    return;
  }

  // [FG] feed actual sample frequency back into config variable
  Mix_QuerySpec(&snd_samplerate, NULL, NULL);

  sound_was_initialized = true;

  Mix_SetPostMix(I_UpdateSound, NULL);

  lprintf(LO_DEBUG, " configured audio device with %d samples/slice\n", audio_buffers);

  I_AtExit(I_ShutdownSound, true, "I_ShutdownSound", exit_priority_normal);

  sfxmutex = SDL_CreateMutex ();

  lprintf(LO_DEBUG, "I_InitSound: sound module ready\n");
  SDL_PauseAudio(0);
}


// NSM sound capture routines

// silences sound output, and instead allows sound capture to work
// call this before sound startup
void I_SetSoundCap (void)
{
  dumping_sound = 1;
}

// grabs len samples of audio (16 bit interleaved)
unsigned char *I_GrabSound (int len)
{
  static unsigned char *buffer = NULL;
  static size_t buffer_size = 0;
  size_t size;

  if (!dumping_sound)
    return NULL;

  size = len * 4;
  if (!buffer || size > buffer_size)
  {
    buffer_size = size * 4;
    buffer = (unsigned char *)Z_Realloc (buffer, buffer_size);
  }

  if (buffer)
  {
    memset (buffer, 0, size);
    I_UpdateSound ((void *) 0xdeadbeef, buffer, size);
  }
  return buffer;
}




// NSM helper routine for some of the streaming audio
void I_ResampleStream (void *dest, unsigned nsamp, void (*proc) (void *dest, unsigned nsamp), unsigned sratein, unsigned srateout)
{ // assumes 16 bit signed interleaved stereo

  unsigned i;
  int j = 0;

  short *sout = (short*)dest;

  static short *sin = NULL;
  static unsigned sinsamp = 0;

  static unsigned remainder = 0;
  unsigned step = (sratein << 16) / (unsigned) srateout;

  unsigned nreq = (step * nsamp + remainder) >> 16;

  if (nreq > sinsamp)
  {
    sin = (short*)Z_Realloc (sin, (nreq + 1) * 4);
    if (!sinsamp) // avoid pop when first starting stream
      sin[0] = sin[1] = 0;
    sinsamp = nreq;
  }

  proc (sin + 2, nreq);

  for (i = 0; i < nsamp; i++)
  {
    *sout++ = ((unsigned) sin[j + 0] * (0x10000 - remainder) +
               (unsigned) sin[j + 2] * remainder) >> 16;
    *sout++ = ((unsigned) sin[j + 1] * (0x10000 - remainder) +
               (unsigned) sin[j + 3] * remainder) >> 16;
    remainder += step;
    j += remainder >> 16 << 1;
    remainder &= 0xffff;
  }
  sin[0] = sin[nreq * 2];
  sin[1] = sin[nreq * 2 + 1];
}
