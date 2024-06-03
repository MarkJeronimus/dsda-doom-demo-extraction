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
 *      The not so system specific sound interface.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __S_SOUND__
#define __S_SOUND__

#include "doomtype.h"
#include "p_mobj.h"
#include "r_defs.h"

#define MAX_CHANNELS 32

//
// Initializes sound stuff, including volume
// Sets channels, SFX volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_Init(void);

// Kills all sounds
void S_Stop(void);

//
// Per level startup code.
// Kills playing sounds at start of level.
//
void S_Start(void);

void S_StartSectorSound(sector_t *sector, int sfx_id);

void S_StartVoidSound(int sfx_id);

// killough 4/25/98: mask used to indicate sound origin is player item pickup
#define PICKUP_SOUND (0x8000)

extern int full_sounds;

void S_AdjustAttenuation(float attenuation);
void S_AdjustVolume(float volume);
void S_ResetAdjustments(void);

// machine-independent sound params
extern int default_numChannels;
extern int numChannels;

// heretic

#include "doomtype.h"

// hexen

int S_GetSoundID(const char *name);

#endif
