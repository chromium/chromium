// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#define UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_

#include <memory>
#include <optional>
#include <string>

#include "cc/paint/paint_flags.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/metadata/view_factory.h"

namespace gfx {
struct VectorIcon;
}

namespace views {

// A native themed class representing a checkbox.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
class VIEWS_EXPORT Checkbox : public LabelButton {
  METADATA_HEADER(Checkbox, LabelButton)

 public:
  explicit Checkbox(const std::u16string& label = std::u16string(),
                    PressedCallback callback = PressedCallback(),
                    int button_context = style::CONTEXT_BUTTON);

  Checkbox(const Checkbox&) = delete;
  Checkbox& operator=(const Checkbox&) = delete;

  ~Checkbox() override;

  // Sets/Gets whether or not the checkbox is checked.
  virtual void SetChecked(bool checked);
  bool GetChecked() const;

  [[nodiscard]] base::CallbackListSubscription AddCheckedChangedCallback(
      PropertyChangedCallback callback);

  void SetMultiLine(bool multi_line);
  bool GetMultiLine() const;

  void SetCheckedIconImageColor(SkColor color);

  // LabelButton:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::ImageSkia GetImage(ButtonState for_state) const override;
  std::unique_ptr<LabelButtonBorder> CreateDefaultBorder() const override;
  std::unique_ptr<ActionViewInterface> GetActionViewInterface() override;

 protected:
  // Bitmask constants for GetIconImageColor.
  enum IconState { CHECKED = 0b1, ENABLED = 0b10 };

  // LabelButton:
  void OnThemeChanged() override;

  // Returns the path to draw the focus ring around for this Checkbox.
  virtual SkPath GetFocusRingPath() const;

  // |icon_state| is a bitmask using the IconState enum.
  // Returns a color for the container portion of the icon.
  virtual SkColor GetIconImageColor(int icon_state) const;
  // Returns a color for the check portion of the icon.
  virtual SkColor GetIconCheckColor(int icon_state) const;

  // Returns a bitmask using the IconState enum.
  int GetIconState(ButtonState for_state) const;

  // Gets the vector icon to use based on the current state of |checked_|.
  virtual const gfx::VectorIcon& GetVectorIcon() const;

 private:
  class FocusRingHighlightPathGenerator;

  // Button:
  void NotifyClick(const ui::Event& event) override;

  ui::NativeTheme::Part GetThemePart() const override;
  void GetExtraParams(ui::NativeTheme::ExtraParams* params) const override;

  void SetAndUpdateAccessibleDefaultActionVerb();

  // True if the checkbox is checked.
  bool checked_ = false;

  std::optional<SkColor> checked_icon_image_color_;
};

class VIEWS_EXPORT CheckboxActionViewInterface
    : public LabelButtonActionViewInterface {
 public:
  explicit CheckboxActionViewInterface(Checkbox* action_view);
  ~CheckboxActionViewInterface() override = default;

  // LabelButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override;
  void OnViewChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<Checkbox> action_view_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Checkbox, LabelButton)
VIEW_BUILDER_PROPERTY(bool, Checked)
VIEW_BUILDER_PROPERTY(bool, MultiLine)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Checkbox)

#endif  // UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
