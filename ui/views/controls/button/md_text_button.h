// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_

#include <memory>

#include "base/optional.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/typography.h"

namespace views {

// A button class that implements the Material Design text button spec.
class VIEWS_EXPORT MdTextButton : public LabelButton {
 public:
  METADATA_HEADER(MdTextButton);

  // As above, but only creates an MdTextButton if MD is enabled in the
  // secondary UI (as opposed to just "top chrome"/"primary" UI).
  static std::unique_ptr<LabelButton> CreateSecondaryUiButton(
      ButtonListener* listener,
      const base::string16& text);
  static std::unique_ptr<LabelButton> CreateSecondaryUiBlueButton(
      ButtonListener* listener,
      const base::string16& text);
  static std::unique_ptr<MdTextButton> Create(
      ButtonListener* listener,
      const base::string16& text,
      int button_context = style::CONTEXT_BUTTON_MD);

  ~MdTextButton() override;

  // See |is_prominent_|.
  void SetProminent(bool is_prominent);
  bool GetProminent() const;

  // See |bg_color_override_|.
  void SetBgColorOverride(const base::Optional<SkColor>& color);
  base::Optional<SkColor> GetBgColorOverride() const;

  // Override the default corner radius of the round rect used for the
  // background and ink drop effects.
  void SetCornerRadius(float radius);
  float GetCornerRadius() const;

  // LabelButton:
  void OnThemeChanged() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  void SetEnabledTextColors(SkColor color) override;
  void SetText(const base::string16& text) override;
  PropertyEffects UpdateStyleToIndicateDefaultStatus() override;
  void StateChanged(ButtonState old_state) override;

 protected:
  // View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;

  MdTextButton(ButtonListener* listener, int button_context);

 private:
  void UpdatePadding();
  void UpdateColors();

  // True if this button uses prominent styling (blue fill, etc.).
  bool is_prominent_;

  // When set, this provides the background color.
  base::Optional<SkColor> bg_color_override_;

  float corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(MdTextButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
