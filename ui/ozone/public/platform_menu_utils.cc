// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/platform_menu_utils.h"

#include "base/notreached.h"

namespace ui {

PlatformMenuUtils::PlatformMenuUtils() = default;

PlatformMenuUtils::~PlatformMenuUtils() = default;

int PlatformMenuUtils::GetCurrentKeyModifiers() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

std::string PlatformMenuUtils::ToDBusKeySym(KeyboardCode code) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

}  // namespace ui
