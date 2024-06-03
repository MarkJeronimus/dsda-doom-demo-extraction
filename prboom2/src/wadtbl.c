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
 *      Ex Demo Wad Table
 *
 *---------------------------------------------------------------------
 */

#include <string.h>

#include "lprintf.h"
#include "m_swap.h"
#include "z_zone.h"

#include "wadtbl.h"

wadinfo_t *ReadPWADTable(char *buffer, size_t size)
{
  int i;
  unsigned int length;
  wadinfo_t *header;
  const filelump_t *fileinfo;

  if (buffer && size > sizeof(*header))
  {
    header = (wadinfo_t*)buffer;
    if (strncmp(header->identification, "IWAD", 4) == 0 ||
        strncmp(header->identification, "PWAD", 4) == 0)
    {
      header->numlumps = LittleLong(header->numlumps);
      header->infotableofs = LittleLong(header->infotableofs);
      length = header->numlumps * sizeof(filelump_t);

      if (header->infotableofs + length <= size)
      {
        fileinfo = (const filelump_t*)(buffer + header->infotableofs);
        for (i = 0; i < header->numlumps; i++, fileinfo++)
        {
          if (fileinfo->filepos < 0 ||
              fileinfo->filepos > header->infotableofs ||
              fileinfo->filepos + fileinfo->size > header->infotableofs)
          {
            break;
          }
        }
        if (i == header->numlumps)
          return header;
      }
    }
  }

  return NULL;
}
