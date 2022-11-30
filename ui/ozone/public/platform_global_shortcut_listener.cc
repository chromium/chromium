// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/platform_global_shortcut_listener.h"

#include "base/check.h"

namespace ui {

PlatformGlobalShortcutListenerDelegate::
    ~PlatformGlobalShortcutListenerDelegate() = default;

PlatformGlobalShortcutListener::PlatformGlobalShortcutListener(
    PlatformGlobalShortcutListenerDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

PlatformGlobalShortcutListener::~PlatformGlobalShortcutListener() = default;

}  // namespace ui
