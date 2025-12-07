// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_

#include <string>

#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/metadata/view_factory.h"

namespace views {

// A native themed class representing a radio button.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
class VIEWS_EXPORT RadioButton : public Checkbox {
  METADATA_HEADER(RadioButton, Checkbox)

 public:
  explicit RadioButton(const std::u16string& label = std::u16string(),
                       int group_id = 0);

  RadioButton(const RadioButton&) = delete;
  RadioButton& operator=(const RadioButton&) = delete;

  ~RadioButton() override;

  // Overridden from View:
  View* GetSelectedViewForGroup(int group) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  bool IsGroupFocusTraversable() const override;
  void OnFocus() override;
  void OnThemeChanged() override;

  // Overridden from Button:
  void RequestFocusFromEvent() override;
  void NotifyClick(const ui::Event& event) override;

  // Overridden from LabelButton:
  gfx::ImageSkia GetImage(ButtonState for_state) const override;
  ui::NativeTheme::Part GetThemePart() const override;

  // Overridden from Checkbox:
  void SetChecked(bool checked) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  SkPath GetFocusRingPath() const override;

 private:
  void GetViewsInGroupFromParent(int group, Views* views);

  bool select_on_focus_ = true;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, RadioButton, Checkbox)
VIEW_BUILDER_PROPERTY(bool, Checked)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, RadioButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
