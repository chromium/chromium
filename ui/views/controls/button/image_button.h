// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

namespace views {

// An image button.
// Note that this type of button is not focusable by default and will not be
// part of the focus chain, unless in accessibility mode. Call
// SetFocusForPlatform() to make it part of the focus chain.
class VIEWS_EXPORT ImageButton : public Button {
 public:
  METADATA_HEADER(ImageButton);

  // An enum describing the horizontal alignment of images on Buttons.
  enum HorizontalAlignment {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT
  };

  // An enum describing the vertical alignment of images on Buttons.
  enum VerticalAlignment {
    ALIGN_TOP = 0,
    ALIGN_MIDDLE,
    ALIGN_BOTTOM
  };

  explicit ImageButton(ButtonListener* listener);
  ~ImageButton() override;

  // Returns the image for a given |state|.
  virtual const gfx::ImageSkia& GetImage(ButtonState state) const;

  // Set the image the button should use for the provided state.
  void SetImage(ButtonState state, const gfx::ImageSkia* image);

  // As above, but takes a const ref. TODO(estade): all callers should be
  // updated to use this version, and then the implementations can be
  // consolidated.
  virtual void SetImage(ButtonState state, const gfx::ImageSkia& image);

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
  void SetDrawImageMirrored(bool mirrored) {
    draw_image_mirrored_ = mirrored;
  }

  // Overridden from View:
  gfx::Size CalculatePreferredSize() const override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;

 protected:
  // Overridden from Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Returns the image to paint. This is invoked from paint and returns a value
  // from images.
  virtual gfx::ImageSkia GetImageToPaint();

  // Updates button background for |scale_factor|.
  void UpdateButtonBackground(ui::ScaleFactor scale_factor);

  // The images used to render the different states of this button.
  gfx::ImageSkia images_[STATE_COUNT];

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

  DISALLOW_COPY_AND_ASSIGN(ImageButton);
};

////////////////////////////////////////////////////////////////////////////////
//
// ToggleImageButton
//
// A toggle-able ImageButton.  It swaps out its graphics when toggled.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT ToggleImageButton : public ImageButton {
 public:
  explicit ToggleImageButton(ButtonListener* listener);
  ~ToggleImageButton() override;

  // Change the toggled state.
  void SetToggled(bool toggled);

  // Like ImageButton::SetImage(), but to set the graphics used for the
  // "has been toggled" state.  Must be called for each button state
  // before the button is toggled.
  void SetToggledImage(ButtonState state, const gfx::ImageSkia* image);

  // Set the tooltip text displayed when the button is toggled.
  void SetToggledTooltipText(const base::string16& tooltip);

  // Overridden from ImageButton:
  const gfx::ImageSkia& GetImage(ButtonState state) const override;
  void SetImage(ButtonState state, const gfx::ImageSkia& image) override;

  // Overridden from View:
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  bool toggled_for_testing() const;

 private:
  // The parent class's images_ member is used for the current images,
  // and this array is used to hold the alternative images.
  // We swap between the two when toggling.
  gfx::ImageSkia alternate_images_[STATE_COUNT];

  // True if the button is currently toggled.
  bool toggled_;

  // The parent class's tooltip_text_ is displayed when not toggled, and
  // this one is shown when toggled.
  base::string16 toggled_tooltip_text_;

  DISALLOW_COPY_AND_ASSIGN(ToggleImageButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_H_
