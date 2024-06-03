//
// Copyright(C) 2022 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA SndInfo
//

#include <stddef.h>

#include "m_misc.h"
#include "sc_man.h"
#include "sounds.h"

#include "dsda/ambient.h"
#include "dsda/map_format.h"

#include "sndinfo.h"

void dsda_LoadSndInfo(void) {
  int i;

  if (!hexen) {
    dsda_LoadAmbientSndInfo();
    return;
  }

  SC_OpenLump("sndinfo");

  while (SC_GetString()) {
    if (*sc_String == '$') {
      if (!strcasecmp(sc_String, "$ARCHIVEPATH"))
        SC_MustGetString();
      else if (!strcasecmp(sc_String, "$MAP")) {
        SC_MustGetNumber();
        SC_MustGetString();
      }

      continue;
    }
    else {
      for (i = 0; i < num_sfx; i++) {
        if (!strcmp(S_sfx[i].tagname, sc_String)) {
          SC_MustGetString();

          if (*sc_String != '?')
            S_sfx[i].name = Z_Strdup(sc_String);
          else
            S_sfx[i].name = Z_Strdup("default");

          break;
        }
      }
      if (i == num_sfx)
        SC_MustGetString();
    }
  }

  SC_Close();

  for (i = 0; i < num_sfx; i++)
    if (!strcmp(S_sfx[i].name, ""))
      S_sfx[i].name = Z_Strdup("default");
}
