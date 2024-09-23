// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EMOJI_EMOJI_PANEL_HELPER_H_
#define UI_BASE_EMOJI_EMOJI_PANEL_HELPER_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "build/chromeos_buildflags.h"

namespace ui {

// Returns whether showing the Emoji Panel is supported on this version of
// the operating system.
COMPONENT_EXPORT(UI_BASE_EMOJI) bool IsEmojiPanelSupported();

// Invokes the commands to show the Emoji Panel.
COMPONENT_EXPORT(UI_BASE_EMOJI) void ShowEmojiPanel();

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Invokes the commands to show the Emoji Panel in tablet mode (ChromeOS only).
COMPONENT_EXPORT(UI_BASE_EMOJI) void ShowTabletModeEmojiPanel();

enum class COMPONENT_EXPORT(UI_BASE_EMOJI) EmojiPickerCategory {
  kEmojis,
  kSymbols,
  kEmoticons,
  kGifs,
};

enum class COMPONENT_EXPORT(UI_BASE_EMOJI) EmojiPickerFocusBehavior {
  kOnlyShowWhenFocused,
  kAlwaysShow,
};

using EmojiKeyboardCallback =
    base::RepeatingCallback<void(EmojiPickerCategory,
                                 EmojiPickerFocusBehavior,
                                 const std::string& initial_query)>;

// Show the emoji picker pre scrolled to a specific category
COMPONENT_EXPORT(UI_BASE_EMOJI)
void ShowEmojiPanelInSpecificMode(EmojiPickerCategory category,
                                  EmojiPickerFocusBehavior focus_behavior,
                                  const std::string& initial_query);

// Sets a callback to show the emoji panel (ChromeOS only).
COMPONENT_EXPORT(UI_BASE_EMOJI)
void SetShowEmojiKeyboardCallback(EmojiKeyboardCallback callback);

// Sets a callback to show the emoji panel in tablet mode (ChromeOS only).
COMPONENT_EXPORT(UI_BASE_EMOJI)
void SetTabletModeShowEmojiKeyboardCallback(base::RepeatingClosure callback);
#endif

}  // namespace ui

#endif  // UI_BASE_EMOJI_EMOJI_PANEL_HELPER_H_
