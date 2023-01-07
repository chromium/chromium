// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_COLOR_CHOOSER_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_COLOR_CHOOSER_EXAMPLE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/color_chooser/color_chooser_view.h"
#include "ui/views/examples/example_base.h"

namespace views::examples {

// A ColorChooser example.
class VIEWS_EXAMPLES_EXPORT ColorChooserExample : public ExampleBase,
                                                  public ColorChooserListener {
 public:
  ColorChooserExample();

  ColorChooserExample(const ColorChooserExample&) = delete;
  ColorChooserExample& operator=(const ColorChooserExample&) = delete;

  ~ColorChooserExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // ColorChooserListener:
  void OnColorChosen(SkColor color) override;
  void OnColorChooserDialogClosed() override;

 private:
  ColorChooser chooser_{this, gfx::kPlaceholderColor};
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_COLOR_CHOOSER_EXAMPLE_H_
