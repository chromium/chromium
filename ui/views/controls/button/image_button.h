// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_H_

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

namespace views {

class VIEWS_EXPORT ImageButton : public Button {
  METADATA_HEADER(ImageButton, Button)

 public:
  // An enum describing the horizontal alignment of images on Buttons.
  enum HorizontalAlignment { ALIGN_LEFT = 0, ALIGN_CENTER, ALIGN_RIGHT };

  // An enum describing the vertical alignment of images on Buttons.
  enum VerticalAlignment { ALIGN_TOP = 0, ALIGN_MIDDLE, ALIGN_BOTTOM };

  explicit ImageButton(PressedCallback callback = PressedCallback());

  ImageButton(const ImageButton&) = delete;
  ImageButton& operator=(const ImageButton&) = delete;

  ~ImageButton() override;

  // Returns the image for a given |state|.
  virtual gfx::ImageSkia GetImage(ButtonState state) const;

  virtual void SetImageModel(ButtonState state,
                             const ui::ImageModel& image_model);

  // Set the background details.  The background image uses the same alignment
  // as the image.
  void SetBackgroundImage(SkColor color,
                          const gfx::ImageSkia* image,
                          const gfx::ImageSkia* mask);

  // How the image is laid out within the button's bounds.
  HorizontalAlignment GetImageHorizontalAlignment() const;
  VerticalAlignment GetImageVerticalAlignment() const;
  void SetImageHorizontalAlignment(HorizontalAlignment h_alignment);
  void SetImageVerticalAlignment(VerticalAlignment v_alignment);

  // The minimum size of the contents (not including the border). The contents
  // will be at least this size, but may be larger if the image itself is
  // larger.
  gfx::Size GetMinimumImageSize() const;
  void SetMinimumImageSize(const gfx::Size& size);

  // Whether we should draw our images resources horizontally flipped.
  void SetDrawImageMirrored(bool mirrored) { draw_image_mirrored_ = mirrored; }

  // Overridden from View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;
  void OnThemeChanged() override;

  enum class MaterialIconStyle { kSmall, kLarge };

  // Static method to create a Icon button with Google Material style
  // guidelines.
  static std::unique_ptr<ImageButton> CreateIconButton(
      PressedCallback callback,
      const gfx::VectorIcon& icon,
      const std::u16string& accessible_name,
      MaterialIconStyle icon_style = MaterialIconStyle::kLarge,
      std::optional<gfx::Insets> insets = std::nullopt);

 protected:
  // Overridden from Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Returns the image to paint. This is invoked from paint and returns a value
  // from images.
  virtual gfx::ImageSkia GetImageToPaint();

  // Updates button background for |scale_factor|.
  void UpdateButtonBackground(ui::ResourceScaleFactor scale_factor);

  // The images used to render the different states of this button.
  std::array<ui::ImageModel, STATE_COUNT> images_;

  gfx::ImageSkia background_image_;

 private:
  FRIEND_TEST_ALL_PREFIXES(ImageButtonTest, Basics);
  FRIEND_TEST_ALL_PREFIXES(ImageButtonTest, ImagePositionWithBorder);
  FRIEND_TEST_ALL_PREFIXES(ImageButtonTest, LeftAlignedMirrored);
  FRIEND_TEST_ALL_PREFIXES(ImageButtonTest, RightAlignedMirrored);
  FRIEND_TEST_ALL_PREFIXES(ImageButtonTest, ImagePositionWithBackground);

  FRIEND_TEST_ALL_PREFIXES(ImageButtonFactoryTest, CreateVectorImageButton);

  // Returns the correct position of the image for painting.
  const gfx::Point ComputeImagePaintPosition(const gfx::ImageSkia& image) const;

  // Image alignment.
  HorizontalAlignment h_alignment_ = ALIGN_LEFT;
  VerticalAlignment v_alignment_ = ALIGN_TOP;
  gfx::Size minimum_image_size_;

  // Whether we draw our resources horizontally flipped. This can happen in the
  // linux titlebar, where image resources were designed to be flipped so a
  // small curved corner in the close button designed to fit into the frame
  // resources.
  bool draw_image_mirrored_ = false;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ImageButton, Button)
VIEW_BUILDER_PROPERTY(bool, DrawImageMirrored)
VIEW_BUILDER_PROPERTY(ImageButton::HorizontalAlignment,
                      ImageHorizontalAlignment)
VIEW_BUILDER_PROPERTY(ImageButton::VerticalAlignment, ImageVerticalAlignment)
VIEW_BUILDER_PROPERTY(gfx::Size, MinimumImageSize)
VIEW_BUILDER_METHOD(SetImageModel, Button::ButtonState, const ui::ImageModel&)

END_VIEW_BUILDER

////////////////////////////////////////////////////////////////////////////////
//
// ToggleImageButton
//
// A toggle-able ImageButton.  It swaps out its graphics when toggled.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT ToggleImageButton : public ImageButton {
  METADATA_HEADER(ToggleImageButton, ImageButton)

 public:
  explicit ToggleImageButton(PressedCallback callback = PressedCallback());

  ToggleImageButton(const ToggleImageButton&) = delete;
  ToggleImageButton& operator=(const ToggleImageButton&) = delete;

  ~ToggleImageButton() override;

  // Change the toggled state.
  bool GetToggled() const;
  void SetToggled(bool toggled);
  void UpdateAccessibleCheckedState();

  // Like ImageButton::SetImage(), but to set the graphics used for the
  // "has been toggled" state.  Must be called for each button state
  // before the button is toggled.
  void SetToggledImage(ButtonState state, const gfx::ImageSkia* image);
  void SetToggledImageModel(ButtonState state,
                            const ui::ImageModel& image_model);

  // Like Views::SetBackground(), but to set the background color used for the
  // "has been toggled" state.
  void SetToggledBackground(std::unique_ptr<Background> b);
  Background* GetToggledBackground() const { return toggled_background_.get(); }

  // Get/Set the tooltip text displayed when the button is toggled.
  std::u16string GetToggledTooltipText() const;
  void SetToggledTooltipText(const std::u16string& tooltip);

  // Get/Set the accessible text used when the button is toggled.
  std::u16string GetToggledAccessibleName() const;
  void SetToggledAccessibleName(const std::u16string& name);

  // Overridden from ImageButton:
  gfx::ImageSkia GetImage(ButtonState state) const override;
  void SetImageModel(ButtonState state,
                     const ui::ImageModel& image_model) override;

  // Overridden from View:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  void UpdateAccessibleRoleIfNeeded();

 private:
  void UpdateAccessibleName();
  // The parent class's images_ member is used for the current images,
  // and this array is used to hold the alternative images.
  // We swap between the two when toggling.
  std::array<ui::ImageModel, STATE_COUNT> alternate_images_;

  // True if the button is currently toggled.
  bool toggled_ = false;

  std::unique_ptr<Background> toggled_background_;

  // The parent class's tooltip_text_ is displayed when not toggled, and
  // this one is shown when toggled.
  std::u16string toggled_tooltip_text_;

  // The parent class's accessibility data is used when not toggled, and this
  // one is used when toggled.
  std::u16string toggled_accessible_name_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ToggleImageButton, ImageButton)
VIEW_BUILDER_PROPERTY(bool, Toggled)
VIEW_BUILDER_PROPERTY(std::unique_ptr<Background>, ToggledBackground)
VIEW_BUILDER_PROPERTY(std::u16string, ToggledTooltipText)
VIEW_BUILDER_PROPERTY(std::u16string, ToggledAccessibleName)
VIEW_BUILDER_METHOD(SetToggledImageModel,
                    Button::ButtonState,
                    const ui::ImageModel&)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ImageButton)
DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ToggleImageButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_H_
