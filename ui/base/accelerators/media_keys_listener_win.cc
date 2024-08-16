// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/media_keys_listener.h"

#include "ui/base/accelerators/global_media_keys_listener_win.h"

namespace ui {

std::unique_ptr<MediaKeysListener> MediaKeysListener::Create(
    MediaKeysListener::Delegate* delegate,
    MediaKeysListener::Scope scope) {
  DCHECK(delegate);

  if (scope == Scope::kGlobal) {
    // We should never have more than one global media keys listener.
    CHECK(!GlobalMediaKeysListenerWin::has_instance());
    return std::make_unique<GlobalMediaKeysListenerWin>(delegate);
  }
  return nullptr;
}

}  // namespace ui
