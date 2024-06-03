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
//	DSDA Features
//

#include <string.h>

#include "z_zone.h"

#include "dsda/utility.h"

#include "features.h"

static uint64_t used_features;

#define FEATURE_BIT(x) ((uint64_t) 1 << x)

void dsda_TrackFeature(int feature) {
  used_features |= FEATURE_BIT(feature);
}

void dsda_ResetFeatures(void) {
  used_features = 0;
}

uint64_t dsda_UsedFeatures(void) {
  return used_features;
}

void dsda_MergeFeatures(uint64_t source) {
  used_features |= source;
}

void dsda_CopyFeatures2(byte* result, uint64_t source) {
  result[0] = (source      ) & 0xff;
  result[1] = (source >>  8) & 0xff;
  result[2] = (source >> 16) & 0xff;
  result[3] = (source >> 24) & 0xff;
  result[4] = (source >> 32) & 0xff;
  result[5] = (source >> 40) & 0xff;
  result[6] = (source >> 48) & 0xff;
  result[7] = (source >> 56) & 0xff;
}
