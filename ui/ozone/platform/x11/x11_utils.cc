// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_utils.h"

#include "ui/base/x/x11_util.h"

namespace ui {

X11Utils::X11Utils() = default;

X11Utils::~X11Utils() = default;

gfx::ImageSkia X11Utils::GetNativeWindowIcon(intptr_t target_window_id) {
  return ui::GetNativeWindowIcon(target_window_id);
}

}  // namespace ui
