// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/media_keys_listener.h"

#include "ui/base/accelerators/system_media_controls_media_keys_listener.h"

namespace ui {

std::unique_ptr<MediaKeysListener> MediaKeysListener::Create(
    MediaKeysListener::Delegate* delegate,
    MediaKeysListener::Scope scope) {
  DCHECK(delegate);

  if (scope == Scope::kGlobal) {
    if (!SystemMediaControlsMediaKeysListener::has_instance()) {
      auto listener =
          std::make_unique<SystemMediaControlsMediaKeysListener>(delegate);
      bool success = listener->Initialize();

      // The Linux implementation should always initialize successfully.
      DCHECK(success);

      return std::move(listener);
    }
    // We shouldn't try to create more than one global MediaKeysListener
    // instance.
    NOTREACHED();
  }
  return nullptr;
}

}  // namespace ui
