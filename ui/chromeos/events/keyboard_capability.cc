// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"

namespace ui {

// static
bool KeyboardCapability::IsSixPackKey(const KeyboardCode& key_code) {
  return base::Contains(kSixPackKeyToSystemKeyMap, key_code);
}

bool KeyboardCapability::IsTopRowKey(const KeyboardCode& key_code) const {
  // A set that includes all top row keys from different keyboards.
  // TODO(longbowei): Now only include top row keys from layout2, add more top
  // row keys from other keyboards in the future.
  static const base::NoDestructor<base::flat_set<KeyboardCode>>
      top_row_action_keys({
          VKEY_BROWSER_BACK,
          VKEY_BROWSER_REFRESH,
          VKEY_ZOOM,
          VKEY_MEDIA_LAUNCH_APP1,
          VKEY_BRIGHTNESS_DOWN,
          VKEY_BRIGHTNESS_UP,
          VKEY_MEDIA_PLAY_PAUSE,
          VKEY_VOLUME_MUTE,
          VKEY_VOLUME_DOWN,
          VKEY_VOLUME_UP,
      });
  return base::Contains(*top_row_action_keys, key_code);
}

}  // namespace ui
