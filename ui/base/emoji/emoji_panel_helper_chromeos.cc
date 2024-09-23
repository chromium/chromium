// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/emoji/emoji_panel_helper.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace ui {

namespace {

EmojiKeyboardCallback& GetShowEmojiKeyboardCallback() {
  static base::NoDestructor<EmojiKeyboardCallback> callback;
  return *callback;
}

base::RepeatingClosure& GetTabletModeShowEmojiKeyboardCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

}  // namespace

bool IsEmojiPanelSupported() {
  // TODO(crbug.com/41416262): Emoji callback is null in Mojo apps because
  // they are in a different process. Fix it and remove the null check.
  return !GetShowEmojiKeyboardCallback().is_null();
}

void ShowEmojiPanel() {
  DCHECK(GetShowEmojiKeyboardCallback());
  GetShowEmojiKeyboardCallback().Run(
      EmojiPickerCategory::kEmojis,
      EmojiPickerFocusBehavior::kOnlyShowWhenFocused,
      /*initial_query=*/std::string());
}

void ShowEmojiPanelInSpecificMode(EmojiPickerCategory category,
                                  EmojiPickerFocusBehavior focus_behavior,
                                  const std::string& initial_query) {
  DCHECK(GetShowEmojiKeyboardCallback());
  GetShowEmojiKeyboardCallback().Run(category, focus_behavior, initial_query);
}

void ShowTabletModeEmojiPanel() {
  DCHECK(GetTabletModeShowEmojiKeyboardCallback());
  GetTabletModeShowEmojiKeyboardCallback().Run();
}

void SetShowEmojiKeyboardCallback(EmojiKeyboardCallback callback) {
  GetShowEmojiKeyboardCallback() = callback;
}

void SetTabletModeShowEmojiKeyboardCallback(base::RepeatingClosure callback) {
  GetTabletModeShowEmojiKeyboardCallback() = callback;
}

}  // namespace ui
