// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EMOJI_EMOJI_PANEL_HELPER_H_
#define UI_BASE_EMOJI_EMOJI_PANEL_HELPER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "build/chromeos_buildflags.h"

namespace ui {

// Returns whether showing the Emoji Panel is supported on this version of
// the operating system.
COMPONENT_EXPORT(UI_BASE) bool IsEmojiPanelSupported();

// Invokes the commands to show the Emoji Panel.
COMPONENT_EXPORT(UI_BASE) void ShowEmojiPanel();

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Sets a callback to show the emoji panel (ChromeOS only).
COMPONENT_EXPORT(UI_BASE)
void SetShowEmojiKeyboardCallback(base::RepeatingClosure callback);
#endif

}  // namespace ui

#endif  // UI_BASE_EMOJI_EMOJI_PANEL_HELPER_H_
