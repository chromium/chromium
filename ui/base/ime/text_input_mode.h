// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_MODE_H_
#define UI_BASE_IME_TEXT_INPUT_MODE_H_

namespace ui {

// This mode corrensponds to inputmode
// http://www.whatwg.org/specs/web-apps/current-work/#attr-fe-inputmode
enum TextInputMode {
  TEXT_INPUT_MODE_DEFAULT,
  TEXT_INPUT_MODE_NONE,
  TEXT_INPUT_MODE_TEXT,
  TEXT_INPUT_MODE_TEL,
  TEXT_INPUT_MODE_URL,
  TEXT_INPUT_MODE_EMAIL,
  TEXT_INPUT_MODE_NUMERIC,
  TEXT_INPUT_MODE_DECIMAL,
  TEXT_INPUT_MODE_SEARCH,

  TEXT_INPUT_MODE_MAX = TEXT_INPUT_MODE_SEARCH,
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_MODE_H_
