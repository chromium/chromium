// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_ACTION_H_
#define UI_BASE_IME_TEXT_INPUT_ACTION_H_

namespace ui {

// This mode corresponds to enterkeyhint
// https://html.spec.whatwg.org/multipage/interaction.html#input-modalities:-the-enterkeyhint-attribute
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base.ime
enum class TextInputAction {
  kDefault,
  kEnter,
  kDone,
  kGo,
  kNext,
  kPrevious,
  kSearch,
  kSend,
  kMaxValue = kSend,
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_ACTION_H_
