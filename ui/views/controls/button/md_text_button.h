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
  // MdTextButton has various button styles that can change the button's
  // background and text color.
  // kDefault: white background with a blue text and a solid outline.
  // kProminent: blue background with white text.
  // kTonal: Cyan background with black text.
  enum class Style {
    kDefault = 0,
    kProminent = 1,
    kTonal = 2,
  };

  METADATA_HEADER(MdTextButton);

  explicit MdTextButton(PressedCallback callback = PressedCallback(),
                        const std::u16string& text = std::u16string(),
                        int button_context = style::CONTEXT_BUTTON_MD);

  MdTextButton(const MdTextButton&) = delete;
  MdTextButton& operator=(const MdTextButton&) = delete;

  ~MdTextButton() override;

  // TODO(crbug.com/1406008): Remove the use of Prominent state and use button
  // style state instead.
  void SetProminent(bool is_prominent);
  bool GetProminent() const;

  void SetStyle(views::MdTextButton::Style button_style);
  Style GetStyle() const;

  // See |bg_color_override_|.
  void SetBgColorOverride(const absl::optional<SkColor>& color);
  absl::optional<SkColor> GetBgColorOverride() const;

  // Override the default corner radius of the round rect used for the
  // background and ink drop effects.
  void SetCornerRadius(absl::optional<float> radius);
  absl::optional<float> GetCornerRadius() const;
  float GetCornerRadiusValue() const;

  // See |custom_padding_|.
  void SetCustomPadding(const absl::optional<gfx::Insets>& padding);
  absl::optional<gfx::Insets> GetCustomPadding() const;

  // LabelButton:
  void OnThemeChanged() override;
  void SetEnabledTextColors(absl::optional<SkColor> color) override;
  void SetText(const std::u16string& text) override;
  PropertyEffects UpdateStyleToIndicateDefaultStatus() override;
  void StateChanged(ButtonState old_state) override;
  void SetImageModel(ButtonState for_state,
                     const ui::ImageModel& image_model) override;

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

  Style style_ = Style::kDefault;

  // When set, this provides the background color.
  absl::optional<SkColor> bg_color_override_;

  // Used to set the corner radius of the button.
  absl::optional<float> corner_radius_;

  // Used to override default padding.
  absl::optional<gfx::Insets> custom_padding_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton, LabelButton)
VIEW_BUILDER_PROPERTY(bool, Prominent)
VIEW_BUILDER_PROPERTY(absl::optional<float>, CornerRadius)
VIEW_BUILDER_PROPERTY(absl::optional<SkColor>, BgColorOverride)
VIEW_BUILDER_PROPERTY(absl::optional<gfx::Insets>, CustomPadding)
VIEW_BUILDER_PROPERTY(MdTextButton::Style, Style)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
