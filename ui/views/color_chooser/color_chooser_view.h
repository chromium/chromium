// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_VIEW_H_
#define UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class ColorChooserListener;
class Textfield;

// ColorChooserView provides the UI to choose a color by mouse and/or keyboard.
// It is typically used for <input type="color">.  Currently the user can
// choose a color by dragging over the bar for hue and the area for saturation
// and value.
class VIEWS_EXPORT ColorChooserView : public WidgetDelegateView,
                                      public TextfieldController {
 public:
  ColorChooserView(ColorChooserListener* listener, SkColor initial_color);
  ~ColorChooserView() override;

  // Called when its color value is changed in the web contents.
  void OnColorChanged(SkColor color);

  // Called when the user chooses a hue from the UI.
  void OnHueChosen(SkScalar hue);

  // Called when the user chooses saturation/value from the UI.
  void OnSaturationValueChosen(SkScalar saturation, SkScalar value);

  float hue() const { return hsv_[0]; }
  float saturation() const { return hsv_[1]; }
  float value() const { return hsv_[2]; }
  void set_listener(ColorChooserListener* listener) { listener_ = listener; }

  View* hue_view_for_testing();
  View* saturation_value_view_for_testing();
  Textfield* textfield_for_testing();
  View* selected_color_patch_for_testing();

  // TextfieldController overrides:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  class HueView;
  class SaturationValueView;
  class SelectedColorPatchView;

  // WidgetDelegate overrides:
  bool CanMinimize() const override;
  View* GetInitiallyFocusedView() override;
  void WindowClosing() override;

  // The current color in HSV coordinate.
  SkScalar hsv_[3];

  // The pointer to the current color chooser for callbacks.  It doesn't take
  // ownership on |listener_| so the user of this class should take care of
  // its lifetime.  See chrome/browser/ui/browser.cc for example.
  ColorChooserListener* listener_;

  // Child views. These are owned as part of the normal views hierarchy.
  // The view of hue chooser.
  HueView* hue_;

  // The view of saturation/value choosing area.
  SaturationValueView* saturation_value_;

  // The textfield to write the color explicitly.
  Textfield* textfield_;

  // The rectangle to denote the selected color.
  SelectedColorPatchView* selected_color_patch_;

  DISALLOW_COPY_AND_ASSIGN(ColorChooserView);
};

}  // namespace views

#endif  // UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_VIEW_H_
