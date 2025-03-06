// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_OZONE_PLATFORM_HEADLESS_H_
#define UI_OZONE_PLATFORM_HEADLESS_OZONE_PLATFORM_HEADLESS_H_

#include "ui/ozone/public/ozone_platform.h"

namespace ui {

class HeadlessWindowManager;

class OzonePlatformHeadless : public OzonePlatform {
 public:
  OzonePlatformHeadless() = default;

  OzonePlatformHeadless(const OzonePlatformHeadless&) = delete;
  OzonePlatformHeadless& operator=(const OzonePlatformHeadless&) = delete;

  ~OzonePlatformHeadless() override = default;

  virtual HeadlessWindowManager* GetHeadlessWindowManager() = 0;
};

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformHeadless();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_OZONE_PLATFORM_HEADLESS_H_
