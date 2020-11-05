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

  explicit MdTextButton(PressedCallback callback = PressedCallback(),
                        const base::string16& text = base::string16(),
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

  // See |custom_padding_|.
  void SetCustomPadding(const base::Optional<gfx::Insets>& padding);
  base::Optional<gfx::Insets> GetCustomPadding() const;

  // LabelButton:
  void OnThemeChanged() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  void SetEnabledTextColors(base::Optional<SkColor> color) override;
  void SetText(const base::string16& text) override;
  PropertyEffects UpdateStyleToIndicateDefaultStatus() override;
  void StateChanged(ButtonState old_state) override;

 protected:
  // View:
  void OnFocus() override;
  void OnBlur() override;

 private:
  void UpdatePadding();
  gfx::Insets CalculateDefaultPadding() const;

  void UpdateTextColor();
  void UpdateBackgroundColor() override;
  void UpdateColors();

  // True if this button uses prominent styling (blue fill, etc.).
  bool is_prominent_ = false;

  // When set, this provides the background color.
  base::Optional<SkColor> bg_color_override_;

  float corner_radius_ = 0.0f;

  // Used to override default padding.
  base::Optional<gfx::Insets> custom_padding_;

  DISALLOW_COPY_AND_ASSIGN(MdTextButton);
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton, LabelButton)
VIEW_BUILDER_PROPERTY(bool, Prominent)
VIEW_BUILDER_PROPERTY(base::Optional<SkColor>, BgColorOverride)
VIEW_BUILDER_PROPERTY(float, CornerRadius)
VIEW_BUILDER_PROPERTY(base::Optional<gfx::Insets>, CustomPadding)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
