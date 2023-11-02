// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/media_keys_listener.h"

namespace ui {

// TODO(ganenkokb): Need implementation for non-mac platforms.
std::unique_ptr<MediaKeysListener> MediaKeysListener::Create(
    MediaKeysListener::Delegate* delegate,
    MediaKeysListener::Scope scope) {
  return nullptr;
}

}  // namespace ui
