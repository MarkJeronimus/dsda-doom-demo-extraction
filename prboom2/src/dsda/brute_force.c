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
//	DSDA Brute Force
//

#include <math.h>

#include "d_player.h"
#include "d_ticcmd.h"
#include "doomstat.h"
#include "lprintf.h"
#include "m_random.h"
#include "r_state.h"

#include "dsda/build.h"
#include "dsda/demo.h"
#include "dsda/features.h"
#include "dsda/key_frame.h"
#include "dsda/skip.h"
#include "dsda/time.h"
#include "dsda/utility.h"

#include "brute_force.h"

#define MAX_BF_DEPTH 35
#define MAX_BF_CONDITIONS 16

typedef struct {
  int min;
  int max;
  int i;
} bf_range_t;

typedef struct {
  dsda_key_frame_t key_frame;
  bf_range_t forwardmove;
  bf_range_t sidemove;
  bf_range_t angleturn;
  byte buttons;
} bf_t;

typedef struct {
  dsda_bf_attribute_t attribute;
  dsda_bf_operator_t operator;
  fixed_t value;
  fixed_t secondary_value;
} bf_condition_t;

typedef struct {
  dsda_bf_attribute_t attribute;
  dsda_bf_limit_t limit;
  fixed_t value;
  dboolean enabled;
  dboolean evaluated;
  fixed_t best_value;
  int best_depth;
  bf_t best_bf[MAX_BF_DEPTH];
} bf_target_t;

static bf_t brute_force[MAX_BF_DEPTH];
static int bf_depth;
static int bf_logictic;
static int bf_condition_count;
static bf_condition_t bf_condition[MAX_BF_CONDITIONS];
static long long bf_volume;
static long long bf_volume_max;
static dboolean bf_mode;
static bf_target_t bf_target;
static ticcmd_t bf_result[MAX_BF_DEPTH];

const char* dsda_bf_attribute_names[dsda_bf_attribute_max] = {
  [dsda_bf_x] = "x",
  [dsda_bf_y] = "y",
  [dsda_bf_z] = "z",
  [dsda_bf_momx] = "vx",
  [dsda_bf_momy] = "vy",
  [dsda_bf_speed] = "spd",
  [dsda_bf_damage] = "dmg",
  [dsda_bf_rng] = "rng",
  [dsda_bf_arm] = "arm",
  [dsda_bf_hp] = "hp",
  [dsda_bf_ammo_0] = "am0",
  [dsda_bf_ammo_1] = "am1",
  [dsda_bf_ammo_2] = "am2",
  [dsda_bf_ammo_3] = "am3",
  [dsda_bf_ammo_4] = "am4",
  [dsda_bf_ammo_5] = "am5",
  [dsda_bf_bmapwidth] = "bmw",
};

static void dsda_RestoreBFKeyFrame(int frame) {
  dsda_RestoreKeyFrame(&brute_force[frame].key_frame, true);
}

static void dsda_StoreBFKeyFrame(int frame) {
  dsda_StoreKeyFrame(&brute_force[frame].key_frame, true, false);
}


#define BF_FAILURE 0
#define BF_SUCCESS 1

static const char* bf_result_text[2] = { "FAILURE", "SUCCESS" };
static dboolean brute_force_ended;

dboolean dsda_BruteForceEnded(void) {
  return brute_force_ended;
}

static void dsda_EndBF(int result) {
  brute_force_ended = true;

  lprintf(LO_INFO, "Brute force complete (%s)!\n", bf_result_text[result]);

  dsda_RestoreBFKeyFrame(0);

  bf_mode = false;

  if (result == BF_SUCCESS)
    dsda_QueueBuildCommands(bf_result, bf_depth);
  else
    dsda_ExitSkipMode();
}

static fixed_t dsda_BFAttribute(int attribute) {
  extern int bmapwidth;

  player_t* player;

  player = &players[displayplayer];

  switch (attribute) {
    case dsda_bf_x:
      return player->mo->x;
    case dsda_bf_y:
      return player->mo->y;
    case dsda_bf_z:
      return player->mo->z;
    case dsda_bf_momx:
      return player->mo->momx;
    case dsda_bf_momy:
      return player->mo->momy;
    case dsda_bf_speed:
      return P_PlayerSpeed(player);
    case dsda_bf_damage:
      {
        extern int player_damage_last_tic;

        return player_damage_last_tic;
      }
    case dsda_bf_rng:
      return rng.rndindex;
    case dsda_bf_arm:
      return player->armorpoints[ARMOR_ARMOR];
    case dsda_bf_hp:
      return player->health;
    case dsda_bf_ammo_0:
      return player->ammo[0];
    case dsda_bf_ammo_1:
      return player->ammo[1];
    case dsda_bf_ammo_2:
      return player->ammo[2];
    case dsda_bf_ammo_3:
      return player->ammo[3];
    case dsda_bf_ammo_4:
      return player->ammo[4];
    case dsda_bf_ammo_5:
      return player->ammo[5];
    case dsda_bf_bmapwidth:
      return bmapwidth;
    default:
      return 0;
  }
}

static dboolean dsda_BFHaveItem(int item) {
  player_t* player;

  player = &players[displayplayer];

  switch (item) {
    case dsda_bf_red_key_card:
      return player->cards[it_redcard];
    case dsda_bf_yellow_key_card:
      return player->cards[it_yellowcard];
    case dsda_bf_blue_key_card:
      return player->cards[it_bluecard];
    case dsda_bf_red_skull_key:
      return player->cards[it_redskull];
    case dsda_bf_yellow_skull_key:
      return player->cards[it_yellowskull];
    case dsda_bf_blue_skull_key:
      return player->cards[it_blueskull];
    case dsda_bf_fist:
      return player->weaponowned[wp_fist];
    case dsda_bf_pistol:
      return player->weaponowned[wp_pistol];
    case dsda_bf_shotgun:
      return player->weaponowned[wp_shotgun];
    case dsda_bf_chaingun:
      return player->weaponowned[wp_chaingun];
    case dsda_bf_rocket_launcher:
      return player->weaponowned[wp_missile];
    case dsda_bf_plasma_gun:
      return player->weaponowned[wp_plasma];
    case dsda_bf_bfg:
      return player->weaponowned[wp_bfg];
    case dsda_bf_chainsaw:
      return player->weaponowned[wp_chainsaw];
    case dsda_bf_super_shotgun:
      return player->weaponowned[wp_supershotgun];
    default:
      return false;
  }
}

static dboolean dsda_BFMiscConditionReached(int i) {
  switch (bf_condition[i].attribute) {
    case dsda_bf_line_skip:
      return lines[bf_condition[i].value].player_activations == bf_condition[i].secondary_value;
    case dsda_bf_line_activation:
      return lines[bf_condition[i].value].player_activations > bf_condition[i].secondary_value;
    case dsda_bf_have_item:
      return dsda_BFHaveItem(bf_condition[i].value);
    case dsda_bf_lack_item:
      return !dsda_BFHaveItem(bf_condition[i].value);
    default:
      return false;
  }
}

static dboolean dsda_BFConditionReached(int i) {
  fixed_t value;

  if (bf_condition[i].operator == dsda_bf_operator_misc)
    return dsda_BFMiscConditionReached(i);

  value = dsda_BFAttribute(bf_condition[i].attribute);

  switch (bf_condition[i].operator) {
    case dsda_bf_less_than:
      return value < bf_condition[i].value;
    case dsda_bf_less_than_or_equal_to:
      return value <= bf_condition[i].value;
    case dsda_bf_greater_than:
      return value > bf_condition[i].value;
    case dsda_bf_greater_than_or_equal_to:
      return value >= bf_condition[i].value;
    case dsda_bf_equal_to:
      return value == bf_condition[i].value;
    case dsda_bf_not_equal_to:
      return value != bf_condition[i].value;
    default:
      return false;
  }
}

static dboolean dsda_BFNewBestResult(fixed_t value) {
  if (!bf_target.evaluated)
    return true;

  switch (bf_target.limit) {
    case dsda_bf_acap:
      return abs(value - bf_target.value) < abs(bf_target.best_value - bf_target.value);
    case dsda_bf_max:
      return value > bf_target.best_value;
    case dsda_bf_min:
      return value < bf_target.best_value;
    default:
      return false;
  }
}

static void dsda_BFEvaluateTarget(void) {
  fixed_t value;

  value = dsda_BFAttribute(bf_target.attribute);

  dsda_BFNewBestResult(value);
}

static dboolean dsda_BFConditionsReached(void) {
  int i, reached;

  reached = 0;
  for (i = 0; i < bf_condition_count; ++i)
    reached += dsda_BFConditionReached(i);

  if (reached == bf_condition_count)
    if (bf_target.enabled) {
      dsda_BFEvaluateTarget();

      return false;
    }

  return reached == bf_condition_count;
}

dboolean dsda_BruteForce(void) {
  return bf_mode;
}

void dsda_UpdateBruteForce(void) {
  int frame;

  frame = true_logictic - bf_logictic;

  if (frame == bf_depth) {
    dsda_RestoreBFKeyFrame(frame);
  }
  else
    dsda_StoreBFKeyFrame(frame);
}

void dsda_EvaluateBruteForce(void) {
  if (true_logictic - bf_logictic != bf_depth)
    return;

  ++bf_volume;

  if (dsda_BFConditionsReached()) {
    dsda_EndBF(BF_SUCCESS);
  }
  else if (bf_volume >= bf_volume_max) {
    if (bf_target.enabled && bf_target.evaluated)
      dsda_EndBF(BF_SUCCESS);
    else
      dsda_EndBF(BF_FAILURE);
  }
}
