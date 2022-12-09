// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_

#include <memory>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/typography.h"

namespace views {

// A button class that implements the Material Design text button spec.
class VIEWS_EXPORT MdTextButton : public LabelButton {
 public:
  METADATA_HEADER(MdTextButton);

  explicit MdTextButton(PressedCallback callback = PressedCallback(),
                        const std::u16string& text = std::u16string(),
                        int button_context = style::CONTEXT_BUTTON_MD);

  MdTextButton(const MdTextButton&) = delete;
  MdTextButton& operator=(const MdTextButton&) = delete;

  ~MdTextButton() override;

  // See |is_prominent_|.
  void SetProminent(bool is_prominent);
  bool GetProminent() const;

  // See |bg_color_override_|.
  void SetBgColorOverride(const absl::optional<SkColor>& color);
  absl::optional<SkColor> GetBgColorOverride() const;

  // Override the default corner radius of the round rect used for the
  // background and ink drop effects.
  void SetCornerRadius(float radius);
  float GetCornerRadius() const;

  // See |custom_padding_|.
  void SetCustomPadding(const absl::optional<gfx::Insets>& padding);
  absl::optional<gfx::Insets> GetCustomPadding() const;

  // LabelButton:
  void OnThemeChanged() override;
  void SetEnabledTextColors(absl::optional<SkColor> color) override;
  void SetText(const std::u16string& text) override;
  PropertyEffects UpdateStyleToIndicateDefaultStatus() override;
  void StateChanged(ButtonState old_state) override;

 protected:
  // View:
  void OnFocus() override;
  void OnBlur() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdatePadding();
  gfx::Insets CalculateDefaultPadding() const;

  void UpdateTextColor();
  void UpdateBackgroundColor() override;
  void UpdateColors();

  // True if this button uses prominent styling (blue fill, etc.).
  bool is_prominent_ = false;

  // When set, this provides the background color.
  absl::optional<SkColor> bg_color_override_;

  float corner_radius_ = 0.0f;

  // Used to override default padding.
  absl::optional<gfx::Insets> custom_padding_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton, LabelButton)
VIEW_BUILDER_PROPERTY(bool, Prominent)
VIEW_BUILDER_PROPERTY(absl::optional<SkColor>, BgColorOverride)
VIEW_BUILDER_PROPERTY(float, CornerRadius)
VIEW_BUILDER_PROPERTY(absl::optional<gfx::Insets>, CustomPadding)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
