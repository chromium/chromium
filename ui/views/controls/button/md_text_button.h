// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_image_container.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/typography.h"

namespace actions {
class ActionItem;
}

namespace views {

// A button class that implements the Material Design text button spec.
class VIEWS_EXPORT MdTextButton : public LabelButton {
  METADATA_HEADER(MdTextButton, LabelButton)

 public:
  explicit MdTextButton(
      PressedCallback callback = PressedCallback(),
      const std::u16string& text = std::u16string(),
      int button_context = style::CONTEXT_BUTTON_MD,
      bool use_text_color_for_icon = true,
      std::unique_ptr<LabelButtonImageContainer> image_container =
          std::make_unique<SingleImageContainer>());

  MdTextButton(const MdTextButton&) = delete;
  MdTextButton& operator=(const MdTextButton&) = delete;

  ~MdTextButton() override;

  void SetStyle(ui::ButtonStyle button_style);
  ui::ButtonStyle GetStyle() const;

  // Sets the background color id to use. Cannot be called if
  // `bg_color_override_` has already been set.
  void SetBgColorIdOverride(const std::optional<ui::ColorId> color_id);
  std::optional<ui::ColorId> GetBgColorIdOverride() const;

  // Sets the background color to use. Cannot be called if
  // `bg_color_id_override_` has already been set.
  // TODO(crbug.com/40259212): Get rid of SkColor versions of these functions in
  // favor of the ColorId versions.
  void SetBgColorOverrideDeprecated(const std::optional<SkColor>& color);
  std::optional<SkColor> GetBgColorOverrideDeprecated() const;

  // Sets the border stroke color id to use.
  void SetStrokeColorIdOverride(const std::optional<ui::ColorId> color_id);
  std::optional<ui::ColorId> GetStrokeColorIdOverride() const;

  // Override the default corner radius (received from the `LayoutProvider` for
  // `ShapeContextTokens::kButtonRadius`) of the round rect used for the
  // background and ink drop effects.
  void SetCornerRadius(std::optional<float> radius);
  std::optional<float> GetCornerRadius() const;
  float GetCornerRadiusValue() const;

  // See |custom_padding_|.
  void SetCustomPadding(const std::optional<gfx::Insets>& padding);
  std::optional<gfx::Insets> GetCustomPadding() const;

  // LabelButton:
  void OnThemeChanged() override;
  void SetEnabledTextColors(std::optional<SkColor> color) override;
  void SetText(const std::u16string& text) override;
  PropertyEffects UpdateStyleToIndicateDefaultStatus() override;
  void StateChanged(ButtonState old_state) override;
  void SetImageModel(ButtonState for_state,
                     const std::optional<ui::ImageModel>& image_model) override;
  std::unique_ptr<ActionViewInterface> GetActionViewInterface() override;

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
  void UpdateIconColor();

  // Returns the hover color depending on the button style.
  SkColor GetHoverColor(ui::ButtonStyle button_style);

  // Updates button attributes that depend on the corner radius.
  void OnCornerRadiusValueChanged();

  ui::ButtonStyle style_ = ui::ButtonStyle::kDefault;

  // When set, this provides the background color. At most one of
  // `bg_color_override_` or `bg_color_id_override_` can be set.
  std::optional<SkColor> bg_color_override_;
  std::optional<ui::ColorId> bg_color_id_override_;

  // When set, this provides the border stroke color.
  std::optional<ui::ColorId> stroke_color_id_override_;

  // Used to set the corner radius of the button.
  std::optional<float> corner_radius_;

  // Used to override default padding.
  std::optional<gfx::Insets> custom_padding_;

  // When set, the icon color will match the text color.
  bool use_text_color_for_icon_ = true;
};

class VIEWS_EXPORT MdTextButtonActionViewInterface
    : public LabelButtonActionViewInterface {
 public:
  explicit MdTextButtonActionViewInterface(MdTextButton* action_view);
  ~MdTextButtonActionViewInterface() override = default;

  // LabelButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<MdTextButton> action_view_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton, LabelButton)
VIEW_BUILDER_PROPERTY(std::optional<float>, CornerRadius)
VIEW_BUILDER_PROPERTY(std::optional<SkColor>, BgColorOverrideDeprecated)
VIEW_BUILDER_PROPERTY(std::optional<ui::ColorId>, BgColorIdOverride)
VIEW_BUILDER_PROPERTY(std::optional<gfx::Insets>, CustomPadding)
VIEW_BUILDER_PROPERTY(ui::ButtonStyle, Style)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, MdTextButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
