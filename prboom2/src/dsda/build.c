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
//	DSDA Build Mode
//

#include "doomstat.h"
#include "g_game.h"

#include "dsda/args.h"
#include "dsda/brute_force.h"
#include "dsda/demo.h"
#include "dsda/exhud.h"
#include "dsda/features.h"
#include "dsda/input.h"
#include "dsda/key_frame.h"
#include "dsda/pause.h"
#include "dsda/playback.h"
#include "dsda/settings.h"
#include "dsda/skip.h"

#include "build.h"

typedef struct {
  ticcmd_t* cmds;
  int depth;
  int original_depth;
} build_cmd_queue_t;

static dboolean build_mode;
static dboolean advance_frame;
static ticcmd_t build_cmd;
static ticcmd_t overwritten_cmd;
static int overwritten_logictic;
static int build_cmd_tic = -1;
static dboolean replace_source = true;
static build_cmd_queue_t cmd_queue;

static signed char forward50(void) {
  return dsda_Flag(dsda_arg_stroller) ?
         pclass[players[consoleplayer].pclass].forwardmove[0] :
         pclass[players[consoleplayer].pclass].forwardmove[1];
}

static signed char strafe40(void) {
  return pclass[players[consoleplayer].pclass].sidemove[1];
}

static signed char strafe50(void) {
  return dsda_Flag(dsda_arg_stroller) ? 0 : forward50();
}

static signed short shortTic(void) {
  return (1 << 8);
}

static signed char maxForward(void) {
  return forward50();
}

static signed char minBackward(void) {
  return -forward50();
}

static signed char maxStrafeRight(void) {
  return strafe50();
}

static signed char minStrafeLeft(void) {
  return -strafe50();
}

void dsda_ChangeBuildCommand(void) {
  if (demoplayback)
    dsda_JoinDemo(NULL);

  replace_source = true;
  build_cmd_tic = true_logictic - 1;
  dsda_JumpToLogicTicFrom(true_logictic, true_logictic - 1);
}

dboolean dsda_BuildMode(void) {
  return build_mode;
}

void dsda_QueueBuildCommands(ticcmd_t* cmds, int depth) {
  cmd_queue.original_depth = depth;
  cmd_queue.depth = depth;

  if (cmd_queue.cmds)
    Z_Free(cmd_queue.cmds);

  cmd_queue.cmds = Z_Malloc(depth * sizeof(*cmds));
  memcpy(cmd_queue.cmds, cmds, depth * sizeof(*cmds));
}

static void dsda_PopCommandQueue(ticcmd_t* cmd) {
  *cmd = cmd_queue.cmds[cmd_queue.original_depth - cmd_queue.depth];
  --cmd_queue.depth;

  if (!cmd_queue.depth)
    dsda_ExitSkipMode();
}

dboolean dsda_BuildPlayback(void) {
  return !replace_source;
}

void dsda_CopyBuildCmd(ticcmd_t* cmd) {
  *cmd = build_cmd;
}

void dsda_ReadBuildCmd(ticcmd_t* cmd) {
  if (cmd_queue.depth)
    dsda_PopCommandQueue(cmd);
  else if (dsda_BruteForce())
    ;
  else if (true_logictic == build_cmd_tic) {
    *cmd = build_cmd;
    build_cmd_tic = -1;
  }
  else
    dsda_CopyPendingCmd(cmd, 0);

  dsda_JoinDemoCmd(cmd);
}

void dsda_EnterBuildMode(void) {
  advance_frame = true;

  if (!true_logictic)
    advance_frame = true;

  build_mode = true;
  dsda_ApplyPauseMode(PAUSE_BUILDMODE);

  dsda_RefreshExHudCommandDisplay();
}

void dsda_RefreshBuildMode(void) {
  if (demoplayback)
    replace_source = false;

  if (!dsda_SkipMode() &&
      overwritten_logictic != true_logictic - 1 &&
      build_cmd_tic == -1 &&
      true_logictic > 0) {
    dsda_CopyPriorCmd(&overwritten_cmd, 1);
    build_cmd = overwritten_cmd;
    overwritten_logictic = true_logictic - 1;
    replace_source = false;
  }
}

dboolean dsda_AdvanceFrame(void) {
  dboolean result;

  if (dsda_SkipMode())
    advance_frame = true;

  result = advance_frame;
  advance_frame = false;

  return result;
}
