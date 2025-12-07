// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_FACTORY_H_
#define UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_FACTORY_H_

#include <memory>
#include <optional>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/views_export.h"

namespace gfx {
struct VectorIcon;
}

namespace views {

class ImageButton;
class ToggleImageButton;

// Creates an ImageButton with an ink drop and a centered image built from a
// vector icon that tracks color changes in NativeTheme.
VIEWS_EXPORT std::unique_ptr<ImageButton>
CreateVectorImageButtonWithNativeTheme(
    Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    std::optional<int> dip_size = std::nullopt,
    ui::ColorId icon_color_id = ui::kColorIcon,
    ui::ColorId icon_disabled_color_id = ui::kColorIconDisabled);

// Creates an ImageButton with an ink drop and a centered image in preparation
// for applying a vector icon with SetImageFromVectorIcon below.
VIEWS_EXPORT std::unique_ptr<ImageButton> CreateVectorImageButton(
    Button::PressedCallback callback);

// Creates a ToggleImageButton with an ink drop and a centered image in
// preparation for applying a vector icon from SetImageFromVectorIcon below.
VIEWS_EXPORT std::unique_ptr<ToggleImageButton> CreateVectorToggleImageButton(
    Button::PressedCallback callback);

// Configures an existing ImageButton with an ink drop and a centered image in
// preparation for applying a vector icon with SetImageFromVectorIcon below.
VIEWS_EXPORT void ConfigureVectorImageButton(ImageButton* button);

// Sets images on |button| for STATE_NORMAL and STATE_DISABLED from the given
// vector icon and colors.
VIEWS_EXPORT void SetImageFromVectorIconWithColor(ImageButton* button,
                                                  const gfx::VectorIcon& icon,
                                                  SkColor icon_color,
                                                  SkColor icon_disabled_color);

// As above, but creates the images at the given size.
VIEWS_EXPORT void SetImageFromVectorIconWithColor(ImageButton* button,
                                                  const gfx::VectorIcon& icon,
                                                  int dip_size,
                                                  SkColor icon_color,
                                                  SkColor icon_disabled_color);

// As above, but sets the toggled images for a toggled image button
// with a given icon color instead of deriving from a text color.
VIEWS_EXPORT void SetToggledImageFromVectorIconWithColor(
    ToggleImageButton* button,
    const gfx::VectorIcon& icon,
    int dip_size,
    SkColor icon_color,
    SkColor disabled_color);

// Sets images on |button| for STATE_NORMAL and STATE_DISABLED with the default
// size from the given vector icon and colors,
VIEWS_EXPORT void SetImageFromVectorIconWithColorId(
    ImageButton* button,
    const gfx::VectorIcon& icon,
    ui::ColorId icon_color_id,
    ui::ColorId icon_disabled_color_id,
    std::optional<int> icon_size = std::nullopt);

// Sets images on a `ToggleImageButton` |button| for STATE_NORMAL and
// STATE_DISABLED with the default size from the given vector icon and colors.
VIEWS_EXPORT void SetToggledImageFromVectorIconWithColorId(
    ToggleImageButton* button,
    const gfx::VectorIcon& icon,
    ui::ColorId icon_color_id,
    ui::ColorId icon_disabled_color_id,
    std::optional<int> icon_size = std::nullopt);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_FACTORY_H_
