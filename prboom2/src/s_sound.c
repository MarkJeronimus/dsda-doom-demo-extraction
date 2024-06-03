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
 * DESCRIPTION:  Platform-independent sound code
 *
 *-----------------------------------------------------------------------------*/

// killough 3/7/98: modified to allow arbitrary listeners in spy mode
// killough 5/2/98: reindented, removed useless code, beautified

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "s_sound.h"
#include "i_sound.h"
#include "i_system.h"
#include "d_main.h"
#include "r_main.h"
#include "m_random.h"
#include "w_wad.h"
#include "lprintf.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "e6y.h"

#include "hexen/sn_sonix.h"

#include "dsda/configuration.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/memory.h"
#include "dsda/settings.h"
#include "dsda/sfx.h"
#include "dsda/skip.h"

// Adjustable by menu.
#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128
#define S_STEREO_SWING (96<<FRACBITS)

const int channel_not_found = -1;

typedef struct
{
  sfxinfo_t *sfxinfo;  // sound information (if null, channel avail.)
  void *origin;        // origin of sound
  int handle;          // handle of the sound being played
  int pitch;

  // heretic
  int priority;

  // hexen
  int volume;

  dboolean active;
  dboolean ambient;
  float attenuation;
  float volume_factor;
  dboolean loop;
  int loop_timeout;
  sfx_class_t sfx_class;
} channel_t;

// the set of channels available
static channel_t channels[MAX_CHANNELS];

// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
int snd_SfxVolume;

// number of channels available
int numChannels;

//
// Internals.
//

void S_StopChannel(int cnum);

static int S_getChannel(void *origin, sfxinfo_t *sfxinfo, sfx_params_t *params);


// heretic
int max_snd_dist = 1600;
int dist_adjust = 160;

static int AmbChan = -1;

void S_ResetSfxVolume(void)
{
  snd_SfxVolume = dsda_IntConfig(dsda_config_sfx_volume);
}

// Initializes sound stuff, including volume
// Sets channels, SFX volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void S_Init(void)
{
  S_Stop();

  numChannels = dsda_IntConfig(dsda_config_snd_channels);
}

void S_Stop(void)
{
  // heretic
  AmbChan = -1;
}

//
// Per level startup code.
// Kills playing sounds at start of level.
//

void S_Start(void)
{
  S_Stop();
}

static float adjust_attenuation;
static float adjust_volume;

void S_AdjustAttenuation(float attenuation) {
  adjust_attenuation = attenuation;
}

void S_AdjustVolume(float volume) {
  adjust_volume = volume;
}

void S_ResetAdjustments(void) {
  adjust_attenuation = 0;
  adjust_volume = 0;
}

void S_StartSectorSound(sector_t *sector, int sfx_id)
{
}

void S_StartMobjSound(mobj_t *mobj, int sfx_id)
{
}

void S_StartVoidSound(int sfx_id)
{
}

// [FG] disable sound cutoffs
int full_sounds;

void S_StopChannel(int cnum)
{
  if (AmbChan == cnum)
    AmbChan = -1;
}

//
// S_getChannel :
//   If none available, return -1.  Otherwise channel #.
//

static int S_ChannelScore(channel_t *channel)
{
  return channel->priority;
}

static int S_LowestScoreChannel(void)
{
  int cnum;
  int lowest_score = INT_MAX;
  int lowest_cnum = channel_not_found;

  for (cnum = 0; cnum < numChannels; ++cnum)
  {
    int score = S_ChannelScore(&channels[cnum]);

    if (score < lowest_score)
    {
      lowest_score = score;
      lowest_cnum = cnum;
    }
  }

  return lowest_cnum;
}

static int S_getChannel(void *origin, sfxinfo_t *sfxinfo, sfx_params_t *params)
{
  return channel_not_found;
}

// heretic

static dboolean S_StopSoundInfo(sfxinfo_t* sfx, sfx_params_t *params)
{
  int i;
  int priority;
  int least_priority;
  int found;

  if (sfx->numchannels == -1)
    return true;

  priority = params->priority;
  least_priority = -1;
  found = 0;

  for (i = 0; i < numChannels; i++)
  {
    if (channels[i].active && channels[i].sfxinfo == sfx && channels[i].origin)
    {
      found++;            //found one.  Now, should we replace it??
      if (priority >= channels[i].priority)
      {                   // if we're gonna kill one, then this'll be it
        if (!channels[i].loop || priority > channels[i].priority)
        {
          least_priority = i;
          priority = channels[i].priority;
        }
      }
    }
  }

  if (found < sfx->numchannels)
    return true;

  if (least_priority >= 0)
  {
    S_StopChannel(least_priority);

    return true;
  }

  return false; // don't replace any sounds
}

static int Raven_S_getChannel(mobj_t *origin, sfxinfo_t *sfx, sfx_params_t *params)
{
  int i;
  static int sndcount = 0;

  for (i = 0; i < numChannels; i++)
  {
    // The sound is already playing
    if (channels[i].active &&
        channels[i].sfxinfo == sfx &&
        channels[i].origin == origin &&
        channels[i].loop && params->loop)
    {
      channels[i].loop_timeout = params->loop_timeout;

      return channel_not_found;
    }
  }

  if (!S_StopSoundInfo(sfx, params))
    return channel_not_found; // other sounds have greater priority

  for (i = 0; i < numChannels; i++)
  {
    if (gamestate != GS_LEVEL || origin == players[displayplayer].mo)
    {
      i = numChannels;
      break;              // let the player have more than one sound.
    }
    if (origin == channels[i].origin)
    {                       // only allow other mobjs one sound
      break;
    }
  }

  if (i >= numChannels)
  {
    // TODO: can ambient sounds even reach this flow?
    if (params->ambient)
    {
      if (AmbChan != -1 && sfx->priority <= channels[AmbChan].sfxinfo->priority)
        return channel_not_found;         //ambient channel already in use

      AmbChan = -1;
    }

    for (i = 0; i < numChannels; i++)
      if (!channels[i].active)
        break;

    if (i >= numChannels)
    {
      int chan;

      //look for a lower priority sound to replace.
      sndcount++;
      if (sndcount >= numChannels)
        sndcount = 0;

      for (chan = 0; chan < numChannels; chan++)
      {
        i = (sndcount + chan) % numChannels;
        if (params->priority >= channels[i].priority)
        {
          chan = -1;  //denote that sound should be replaced.
          break;
        }
      }

      if (chan != -1)
        return channel_not_found;  //no free channels.

      S_StopChannel(i);
    }
  }

  return i;
}

// hexen

int S_GetSoundID(const char *name)
{
    int i;

    for (i = 0; i < num_sfx; i++)
    {
        if (!strcmp(S_sfx[i].tagname, name))
        {
            return i;
        }
    }
    return 0;
}
