// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_VIEW_H_
#define UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

namespace views {

class ColorChooserListener;
class Textfield;
class WidgetDelegate;

class HueView;
class SaturationValueView;
class SelectedColorPatchView;

// ColorChooser provides the UI to choose a color by mouse and/or keyboard.
// It is typically used for <input type="color">.  Currently the user can
// choose a color by dragging over the bar for hue and the area for saturation
// and value.
//
// All public methods on ColorChooser are safe to call before, during, or after
// the existence of the corresponding Widget/Views/etc.
class VIEWS_EXPORT ColorChooser final : public TextfieldController {
 public:
  ColorChooser(ColorChooserListener* listener, SkColor initial_color);
  ~ColorChooser() override;

  // Construct the WidgetDelegate that should be used to show the actual dialog
  // for this ColorChooser. It is only safe to call this once per ColorChooser
  // instance.
  std::unique_ptr<WidgetDelegate> MakeWidgetDelegate();

  SkColor GetColor() const;
  SkScalar hue() const { return hsv_[0]; }
  SkScalar saturation() const { return hsv_[1]; }
  SkScalar value() const { return hsv_[2]; }

  bool IsViewAttached() const;

  // Called when its color value is changed in the web contents.
  void OnColorChanged(SkColor color);

  // TextfieldController overrides, public for testing:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  View* hue_view_for_testing();
  View* saturation_value_view_for_testing();
  Textfield* textfield_for_testing();
  View* selected_color_patch_for_testing();

 private:
  std::unique_ptr<View> BuildView();

  void SetColor(SkColor color);
  void SetHue(SkScalar hue);
  void SetSaturationValue(SkScalar saturation, SkScalar value);

  void OnViewClosing();

  // Called when the user chooses a hue from the UI.
  void OnHueChosen(SkScalar hue);

  // Called when the user chooses saturation/value from the UI.
  void OnSaturationValueChosen(SkScalar saturation, SkScalar value);

  // The current color in HSV coordinate.
  std::array<SkScalar, 3> hsv_;

  raw_ptr<ColorChooserListener> listener_;
  ViewTracker tracker_;

  // Child views. These are owned as part of the normal views hierarchy.
  // The view of hue chooser.
  raw_ptr<HueView> hue_ = nullptr;

  // The view of saturation/value choosing area.
  raw_ptr<SaturationValueView> saturation_value_ = nullptr;

  // The rectangle to denote the selected color.
  raw_ptr<SelectedColorPatchView> selected_color_patch_ = nullptr;

  // The textfield to write the color explicitly.
  raw_ptr<Textfield> textfield_ = nullptr;

  SkColor initial_color_;

  base::WeakPtrFactory<ColorChooser> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_VIEW_H_
