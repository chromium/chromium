// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform_selection.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/ozone/platform_list.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

// Returns the name of the platform to use (value of --ozone-platform flag).
std::string GetPlatformName() {
  // The first platform is the default.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOzonePlatform) &&
      kPlatformCount > 0)
    return kPlatformNames[0];
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kOzonePlatform);
}

int g_selected_platform = -1;

}  // namespace

int GetOzonePlatformId() {
  if (g_selected_platform >= 0)
    return g_selected_platform;

  std::string platform_name = GetPlatformName();

  // Search for a matching platform in the list.
  for (int platform_id = 0; platform_id < kPlatformCount; ++platform_id) {
    if (platform_name == kPlatformNames[platform_id]) {
      g_selected_platform = platform_id;
      return g_selected_platform;
    }
  }

  LOG(FATAL) << "Invalid ozone platform: " << platform_name;
  return -1;  // not reached
}

const char* GetOzonePlatformName() {
  return kPlatformNames[GetOzonePlatformId()];
}

}  // namespace ui
