// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_delegate_base.h"

#include "ui/gfx/geometry/size.h"

namespace ui {

PlatformWindowDelegateBase::PlatformWindowDelegateBase() = default;

PlatformWindowDelegateBase::~PlatformWindowDelegateBase() = default;

base::Optional<gfx::Size>
PlatformWindowDelegateBase::GetMinimumSizeForWindow() {
  return base::nullopt;
}

base::Optional<gfx::Size>
PlatformWindowDelegateBase::GetMaximumSizeForWindow() {
  return base::nullopt;
}

}  // namespace ui
