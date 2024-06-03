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
#include "e6y.h"
#include "dsda/settings.h"

void I_InitSoundParams(void)
{
}
