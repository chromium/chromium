// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#define UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "cc/paint/paint_flags.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"

namespace gfx {
struct VectorIcon;
}

namespace views {

// A native themed class representing a checkbox.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
class VIEWS_EXPORT Checkbox : public LabelButton {
 public:
  METADATA_HEADER(Checkbox);

  // |force_md| forces MD even when --secondary-ui-md flag is not set.
  explicit Checkbox(const base::string16& label,
                    ButtonListener* listener = nullptr);
  ~Checkbox() override;

  // Sets/Gets whether or not the checkbox is checked.
  virtual void SetChecked(bool checked);
  bool GetChecked() const;

  void SetMultiLine(bool multi_line);
  bool GetMultiLine() const;

  // If the accessible name should be the same as the labelling view's text,
  // use this. It will set the accessible label relationship and copy the
  // accessible name from the labelling views's accessible name. Any view with
  // an accessible name can be used, e.g. a Label, StyledLabel or Link.
  void SetAssociatedLabel(View* labelling_view);

  // LabelButton:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 protected:
  // LabelButton:
  void OnThemeChanged() override;
  std::unique_ptr<InkDrop> CreateInkDrop() override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropMask> CreateInkDropMask() const override;
  SkColor GetInkDropBaseColor() const override;
  gfx::ImageSkia GetImage(ButtonState for_state) const override;
  std::unique_ptr<LabelButtonBorder> CreateDefaultBorder() const override;

  // Gets the vector icon to use based on the current state of |checked_|.
  virtual const gfx::VectorIcon& GetVectorIcon() const;

  // Returns the path to draw the focus ring around for this Checkbox.
  virtual SkPath GetFocusRingPath() const;

 private:
  class FocusRingHighlightPathGenerator;

  // Bitmask constants for GetIconImageColor.
  enum IconState { CHECKED = 0b1, ENABLED = 0b10 };

  // |icon_state| is a bitmask using the IconState enum.
  SkColor GetIconImageColor(int icon_state) const;

  // Button:
  void NotifyClick(const ui::Event& event) override;

  ui::NativeTheme::Part GetThemePart() const override;
  void GetExtraParams(ui::NativeTheme::ExtraParams* params) const override;

  // True if the checkbox is checked.
  bool checked_;

  // The unique id for the associated label's accessible object.
  int32_t label_ax_id_;

  DISALLOW_COPY_AND_ASSIGN(Checkbox);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
