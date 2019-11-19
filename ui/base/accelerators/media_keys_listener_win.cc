// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/media_keys_listener.h"

#include "ui/base/accelerators/global_media_keys_listener_win.h"
#include "ui/base/accelerators/system_media_controls_media_keys_listener.h"

namespace ui {

std::unique_ptr<MediaKeysListener> MediaKeysListener::Create(
    MediaKeysListener::Delegate* delegate,
    MediaKeysListener::Scope scope) {
  DCHECK(delegate);

  if (scope == Scope::kGlobal) {
    // We should never have more than one global media keys listener.
    if (!SystemMediaControlsMediaKeysListener::has_instance() &&
        !GlobalMediaKeysListenerWin::has_instance()) {
      auto listener =
          std::make_unique<SystemMediaControlsMediaKeysListener>(delegate);
      if (listener->Initialize())
        return listener;

      // If |Initialize()| fails, then we fall back to the
      // GlobalMediaKeysListenerWin.
      return std::make_unique<GlobalMediaKeysListenerWin>(delegate);
    }
    NOTREACHED();
  }
  return nullptr;
}

}  // namespace ui