// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_BUTTON_STICKER_SHEET_H_
#define UI_VIEWS_EXAMPLES_BUTTON_STICKER_SHEET_H_

#include "ui/views/examples/example_base.h"

namespace views::examples {

// An "example" that displays a sticker sheet of all the available material
// design button styles. This example only looks right with `--secondary-ui-md`.
// It is designed to be as visually similar to the UI Harmony spec's sticker
// sheet for buttons as possible.
class VIEWS_EXAMPLES_EXPORT ButtonStickerSheet : public ExampleBase {
 public:
  ButtonStickerSheet();

  ButtonStickerSheet(const ButtonStickerSheet&) = delete;
  ButtonStickerSheet& operator=(const ButtonStickerSheet&) = delete;

  ~ButtonStickerSheet() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_BUTTON_STICKER_SHEET_H_
