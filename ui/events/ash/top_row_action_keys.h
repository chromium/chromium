// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_TOP_ROW_ACTION_KEYS_H_
#define UI_EVENTS_ASH_TOP_ROW_ACTION_KEYS_H_

namespace ui {

// TODO(dpad): Handle display mirror top row keys.
// This enum should mirror the enum `KeyboardTopRowLayout` in
// tools/metrics/histograms/enums.xml and values should not be changed.
#define TOP_ROW_ACTION_KEYS                          \
  TOP_ROW_ACTION_KEYS_ENTRY(None)                    \
  TOP_ROW_ACTION_KEYS_ENTRY(Unknown)                 \
  TOP_ROW_ACTION_KEYS_ENTRY(Back)                    \
  TOP_ROW_ACTION_KEYS_ENTRY(Forward)                 \
  TOP_ROW_ACTION_KEYS_ENTRY(Refresh)                 \
  TOP_ROW_ACTION_KEYS_ENTRY(Fullscreen)              \
  TOP_ROW_ACTION_KEYS_ENTRY(Overview)                \
  TOP_ROW_ACTION_KEYS_ENTRY(Screenshot)              \
  TOP_ROW_ACTION_KEYS_ENTRY(ScreenBrightnessDown)    \
  TOP_ROW_ACTION_KEYS_ENTRY(ScreenBrightnessUp)      \
  TOP_ROW_ACTION_KEYS_ENTRY(MicrophoneMute)          \
  TOP_ROW_ACTION_KEYS_ENTRY(VolumeMute)              \
  TOP_ROW_ACTION_KEYS_ENTRY(VolumeDown)              \
  TOP_ROW_ACTION_KEYS_ENTRY(VolumeUp)                \
  TOP_ROW_ACTION_KEYS_ENTRY(KeyboardBacklightToggle) \
  TOP_ROW_ACTION_KEYS_ENTRY(KeyboardBacklightDown)   \
  TOP_ROW_ACTION_KEYS_ENTRY(KeyboardBacklightUp)     \
  TOP_ROW_ACTION_KEYS_ENTRY(NextTrack)               \
  TOP_ROW_ACTION_KEYS_ENTRY(PreviousTrack)           \
  TOP_ROW_ACTION_KEYS_ENTRY(PlayPause)               \
  TOP_ROW_ACTION_KEYS_ENTRY(AllApplications)         \
  TOP_ROW_ACTION_KEYS_ENTRY(EmojiPicker)             \
  TOP_ROW_ACTION_KEYS_ENTRY(Dictation)               \
  TOP_ROW_ACTION_KEYS_ENTRY(PrivacyScreenToggle)     \
  TOP_ROW_ACTION_KEYS_LAST_ENTRY(Accessibility)

enum class TopRowActionKey {
#define TOP_ROW_ACTION_KEYS_ENTRY(top_row_key) k##top_row_key,
#define TOP_ROW_ACTION_KEYS_LAST_ENTRY(top_row_key) \
  k##top_row_key, kMaxValue = k##top_row_key,
  TOP_ROW_ACTION_KEYS
#undef TOP_ROW_ACTION_KEYS_LAST_ENTRY
#undef TOP_ROW_ACTION_KEYS_ENTRY
};

const char* GetTopRowActionKeyName(TopRowActionKey action);

}  // namespace ui

#endif  // UI_EVENTS_ASH_TOP_ROW_ACTION_KEYS_H_
