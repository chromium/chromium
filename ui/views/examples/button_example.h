// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/examples/example_base.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ui {
class Event;
}  // namespace ui

namespace views {

class ImageButton;
class LabelButton;
class MdTextButton;

namespace examples {

// ButtonExample simply counts the number of clicks.
class VIEWS_EXAMPLES_EXPORT ButtonExample : public ExampleBase {
 public:
  ButtonExample();

  ButtonExample(const ButtonExample&) = delete;
  ButtonExample& operator=(const ButtonExample&) = delete;

  ~ButtonExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void LabelButtonPressed(LabelButton* label_button, const ui::Event& event);
  void ImageButtonPressed();

  // Example buttons.
  LabelButton* label_button_ = nullptr;
  MdTextButton* md_button_ = nullptr;
  MdTextButton* md_disabled_button_ = nullptr;
  MdTextButton* md_default_button_ = nullptr;
  MdTextButton* md_tonal_button_ = nullptr;
  ImageButton* image_button_ = nullptr;

  raw_ptr<const gfx::ImageSkia> icon_ = nullptr;

  // The number of times the buttons are pressed.
  int count_ = 0;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
