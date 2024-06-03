/* Emacs style mode select   -*- C -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze, Andrey Budko
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
 *      Uncapped framerate stuff
 *
 *---------------------------------------------------------------------
 */

#include "doomstat.h"
#include "m_random.h"
#include "r_defs.h"
#include "r_state.h"
#include "p_spec.h"
#include "r_fps.h"
#include "i_system.h"
#include "e6y.h"

#include "dsda/aim.h"
#include "dsda/build.h"
#include "dsda/configuration.h"
#include "dsda/pause.h"
#include "dsda/scroll.h"
#include "dsda/settings.h"

#include "hexen/a_action.h"

dboolean isExtraDDisplay = false;

typedef enum
{
  INTERP_SectorFloor,
  INTERP_SectorCeiling,
  INTERP_WallPanning,
  INTERP_FloorPanning,
  INTERP_CeilingPanning
} interpolation_type_e;

typedef struct
{
  interpolation_type_e type;
  void *address;
} interpolation_t;

tic_vars_t tic_vars;

static void R_DoAnInterpolation (int i, fixed_t smoothratio);

void D_Display();

typedef fixed_t fixed2_t[2];
static fixed2_t *oldipos;
static fixed2_t *bakipos;
static interpolation_t *curipos;

void R_InterpolateView(player_t *player, fixed_t frac)
{
  static mobj_t *oviewer;
  int quake_intensity;

  dboolean NoInterpolate = dsda_CameraPaused() || dsda_PausedViaMenu();

  quake_intensity = dsda_IntConfig(dsda_config_quake_intensity);

  viewplayer = player;

  if (player->mo != oviewer || NoInterpolate)
  {
    oviewer = player->mo;
  }

  if (NoInterpolate)
    frac = FRACUNIT;
  tic_vars.frac = frac;

  if (walkcamera.type != 2)
  {
    viewx = player->mo->x;
    viewy = player->mo->y;
    viewz = player->viewz;
  }
  else
  {
    viewx = walkcamera.x;
    viewy = walkcamera.y;
    viewz = walkcamera.z;
  }
  if (walkcamera.type)
  {
    viewangle = walkcamera.angle;
    viewpitch = walkcamera.pitch;
  }
  else
  {
    viewangle = player->mo->angle;
    viewpitch = dsda_PlayerPitch(player);
  }

  if (localQuakeHappening[displayplayer] && !dsda_Paused())
  {
    static int x_displacement;
    static int y_displacement;
    static int last_leveltime = -1;

    if (leveltime != last_leveltime)
    {
      int intensity = localQuakeHappening[displayplayer];

      x_displacement = ((M_Random() % (intensity << 2)) - (intensity << 1)) << FRACBITS;
      y_displacement = ((M_Random() % (intensity << 2)) - (intensity << 1)) << FRACBITS;

      x_displacement = x_displacement * quake_intensity / 100;
      y_displacement = y_displacement * quake_intensity / 100;

      last_leveltime = leveltime;
    }

    viewx += x_displacement;
    viewy += y_displacement;
  }
}

static void R_CopyInterpToOld (int i)
{
  switch (curipos[i].type)
  {
  case INTERP_SectorFloor:
    oldipos[i][0] = ((sector_t*)curipos[i].address)->floorheight;
    break;
  case INTERP_SectorCeiling:
    oldipos[i][0] = ((sector_t*)curipos[i].address)->ceilingheight;
    break;
  case INTERP_WallPanning:
    oldipos[i][0] = ((side_t*)curipos[i].address)->rowoffset;
    oldipos[i][1] = ((side_t*)curipos[i].address)->textureoffset;
    break;
  case INTERP_FloorPanning:
    oldipos[i][0] = ((sector_t*)curipos[i].address)->floor_xoffs;
    oldipos[i][1] = ((sector_t*)curipos[i].address)->floor_yoffs;
    break;
  case INTERP_CeilingPanning:
    oldipos[i][0] = ((sector_t*)curipos[i].address)->ceiling_xoffs;
    oldipos[i][1] = ((sector_t*)curipos[i].address)->ceiling_yoffs;
    break;
  }
}

static void R_CopyBakToInterp (int i)
{
  switch (curipos[i].type)
  {
  case INTERP_SectorFloor:
    ((sector_t*)curipos[i].address)->floorheight = bakipos[i][0];
    break;
  case INTERP_SectorCeiling:
    ((sector_t*)curipos[i].address)->ceilingheight = bakipos[i][0];
    break;
  case INTERP_WallPanning:
    ((side_t*)curipos[i].address)->rowoffset = bakipos[i][0];
    ((side_t*)curipos[i].address)->textureoffset = bakipos[i][1];
    break;
  case INTERP_FloorPanning:
    ((sector_t*)curipos[i].address)->floor_xoffs = bakipos[i][0];
    ((sector_t*)curipos[i].address)->floor_yoffs = bakipos[i][1];
    break;
  case INTERP_CeilingPanning:
    ((sector_t*)curipos[i].address)->ceiling_xoffs = bakipos[i][0];
    ((sector_t*)curipos[i].address)->ceiling_yoffs = bakipos[i][1];
    break;
  }
}

static void R_DoAnInterpolation (int i, fixed_t smoothratio)
{
  fixed_t pos;
  fixed_t *adr1 = NULL;
  fixed_t *adr2 = NULL;

  switch (curipos[i].type)
  {
  case INTERP_SectorFloor:
    adr1 = &((sector_t*)curipos[i].address)->floorheight;
    break;
  case INTERP_SectorCeiling:
    adr1 = &((sector_t*)curipos[i].address)->ceilingheight;
    break;
  case INTERP_WallPanning:
    adr1 = &((side_t*)curipos[i].address)->rowoffset;
    adr2 = &((side_t*)curipos[i].address)->textureoffset;
    break;
  case INTERP_FloorPanning:
    adr1 = &((sector_t*)curipos[i].address)->floor_xoffs;
    adr2 = &((sector_t*)curipos[i].address)->floor_yoffs;
    break;
  case INTERP_CeilingPanning:
    adr1 = &((sector_t*)curipos[i].address)->ceiling_xoffs;
    adr2 = &((sector_t*)curipos[i].address)->ceiling_yoffs;
    break;

 default:
    return;
  }

  if (adr1)
  {
    pos = bakipos[i][0] = *adr1;
    *adr1 = oldipos[i][0] + FixedMul (pos - oldipos[i][0], smoothratio);
  }

  if (adr2)
  {
    pos = bakipos[i][1] = *adr2;
    *adr2 = oldipos[i][1] + FixedMul (pos - oldipos[i][1], smoothratio);
  }

  switch (curipos[i].type)
  {
  case INTERP_SectorFloor:
  case INTERP_SectorCeiling:
    gld_UpdateSplitData(((sector_t*)curipos[i].address));
    break;
  }
}

static void R_InterpolationGetData(thinker_t *th,
  interpolation_type_e *type1, interpolation_type_e *type2,
  void **posptr1, void **posptr2)
{
  *posptr1 = NULL;
  *posptr2 = NULL;

  if (th->function == T_MoveFloor)
  {
    *type1 = INTERP_SectorFloor;
    *posptr1 = ((floormove_t *)th)->sector;
  }
  else
  if (th->function == T_PlatRaise)
  {
    *type1 = INTERP_SectorFloor;
    *posptr1 = ((plat_t *)th)->sector;
  }
  else
  if (th->function == T_MoveCeiling)
  {
    *type1 = INTERP_SectorCeiling;
    *posptr1 = ((ceiling_t *)th)->sector;
  }
  else
  if (th->function == T_VerticalDoor)
  {
    *type1 = INTERP_SectorCeiling;
    *posptr1 = ((vldoor_t *)th)->sector;
  }
  else
  if (th->function == T_MoveElevator)
  {
    *type1 = INTERP_SectorFloor;
    *posptr1 = ((elevator_t *)th)->sector;
    *type2 = INTERP_SectorCeiling;
    *posptr2 = ((elevator_t *)th)->sector;
  }
  else
  if (th->function == dsda_UpdateSideScroller || th->function == dsda_UpdateControlSideScroller)
  {
    *type1 = INTERP_WallPanning;
    *posptr1 = sides + ((scroll_t *)th)->affectee;
  }
  else
  if (th->function == dsda_UpdateFloorScroller || th->function == dsda_UpdateControlFloorScroller)
  {
    *type1 = INTERP_FloorPanning;
    *posptr1 = sectors + ((scroll_t *)th)->affectee;
  }
  else
  if (th->function == dsda_UpdateCeilingScroller || th->function == dsda_UpdateControlCeilingScroller)
  {
    *type1 = INTERP_CeilingPanning;
    *posptr1 = sectors + ((scroll_t *)th)->affectee;
  }
}
