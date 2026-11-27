// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_RENDER_TEXT_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_RENDER_TEXT_EXAMPLE_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Checkbox;
class Combobox;
class Label;
class Textfield;

namespace examples {

// An example that draws text with gfx::RenderText and lets the user change
// its parameters: font, color, eliding, alignment, display rect and display
// offset.
class VIEWS_EXAMPLES_EXPORT RenderTextExample : public ExampleBase {
 public:
  RenderTextExample();

  RenderTextExample(const RenderTextExample&) = delete;
  RenderTextExample& operator=(const RenderTextExample&) = delete;

  ~RenderTextExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  class RenderTextView;

  // Copies the selected sample text into `text_field_`.
  void OnSampleChanged();

  // Applies the state of all controls to the RenderText and repaints it.
  void Update();

  // Text.
  raw_ptr<Combobox> sample_cb_ = nullptr;
  raw_ptr<Textfield> text_field_ = nullptr;

  // Font.
  raw_ptr<Combobox> font_cb_ = nullptr;
  raw_ptr<Textfield> font_size_field_ = nullptr;
  raw_ptr<Combobox> weight_cb_ = nullptr;
  raw_ptr<Checkbox> italic_checkbox_ = nullptr;
  raw_ptr<Checkbox> underline_checkbox_ = nullptr;
  raw_ptr<Checkbox> strike_checkbox_ = nullptr;

  // Color.
  raw_ptr<Combobox> color_cb_ = nullptr;
  raw_ptr<Combobox> range_color_cb_ = nullptr;
  raw_ptr<Textfield> color_range_start_field_ = nullptr;
  raw_ptr<Textfield> color_range_end_field_ = nullptr;

  // Eliding and line breaking.
  raw_ptr<Combobox> elide_cb_ = nullptr;
  raw_ptr<Combobox> whitespace_elision_cb_ = nullptr;
  raw_ptr<Checkbox> multiline_checkbox_ = nullptr;
  raw_ptr<Textfield> max_lines_field_ = nullptr;
  raw_ptr<Combobox> word_wrap_cb_ = nullptr;

  // Alignment, cursor and clipping.
  raw_ptr<Combobox> h_align_cb_ = nullptr;
  raw_ptr<Combobox> v_align_cb_ = nullptr;
  raw_ptr<Checkbox> cursor_checkbox_ = nullptr;
  raw_ptr<Checkbox> clip_checkbox_ = nullptr;

  // Display rect, in the coordinates of `text_view_`.
  raw_ptr<Checkbox> fit_checkbox_ = nullptr;
  raw_ptr<Textfield> rect_x_field_ = nullptr;
  raw_ptr<Textfield> rect_y_field_ = nullptr;
  raw_ptr<Textfield> rect_width_field_ = nullptr;
  raw_ptr<Textfield> rect_height_field_ = nullptr;

  // Display offset.
  raw_ptr<Textfield> offset_x_field_ = nullptr;
  raw_ptr<Textfield> offset_y_field_ = nullptr;

  // Shows values computed by the RenderText.
  raw_ptr<Label> status_label_ = nullptr;

  // Draws the RenderText.
  raw_ptr<RenderTextView> text_view_ = nullptr;

  std::vector<base::CallbackListSubscription> subscriptions_;

  base::WeakPtrFactory<RenderTextExample> weak_ptr_factory_{this};
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_RENDER_TEXT_EXAMPLE_H_
