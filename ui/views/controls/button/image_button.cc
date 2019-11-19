// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_button.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace views {

// Default button size if no image is set. This is ignored if there is an image,
// and exists for historical reasons (any number of clients could depend on this
// behaviour).
static constexpr int kDefaultWidth = 16;
static constexpr int kDefaultHeight = 14;

////////////////////////////////////////////////////////////////////////////////
// ImageButton, public:

ImageButton::ImageButton(ButtonListener* listener) : Button(listener) {
  // By default, we request that the gfx::Canvas passed to our View::OnPaint()
  // implementation is flipped horizontally so that the button's images are
  // mirrored when the UI directionality is right-to-left.
  EnableCanvasFlippingForRTLUI(true);
}

ImageButton::~ImageButton() = default;

const gfx::ImageSkia& ImageButton::GetImage(ButtonState state) const {
  return images_[state];
}

void ImageButton::SetImage(ButtonState for_state, const gfx::ImageSkia* image) {
  SetImage(for_state, image ? *image : gfx::ImageSkia());
}

void ImageButton::SetImage(ButtonState for_state, const gfx::ImageSkia& image) {
  if (for_state == STATE_HOVERED)
    set_animate_on_state_change(!image.isNull());
  const gfx::Size old_preferred_size = GetPreferredSize();
  images_[for_state] = image;

  if (old_preferred_size != GetPreferredSize())
    PreferredSizeChanged();

  // Even if |for_state| isn't the current state this image could be painted;
  // see |GetImageToPaint()|. So, always repaint.
  SchedulePaint();
}

void ImageButton::SetBackgroundImage(SkColor color,
                                     const gfx::ImageSkia* image,
                                     const gfx::ImageSkia* mask) {
  if (image == nullptr || mask == nullptr) {
    background_image_ = gfx::ImageSkia();
    return;
  }

  background_image_ = gfx::ImageSkiaOperations::CreateButtonBackground(color,
     *image, *mask);
}

ImageButton::HorizontalAlignment ImageButton::GetImageHorizontalAlignment()
    const {
  return h_alignment_;
}

ImageButton::VerticalAlignment ImageButton::GetImageVerticalAlignment() const {
  return v_alignment_;
}

void ImageButton::SetImageHorizontalAlignment(HorizontalAlignment h_alignment) {
  if (GetImageHorizontalAlignment() == h_alignment)
    return;
  h_alignment_ = h_alignment;
  OnPropertyChanged(&h_alignment_, kPropertyEffectsPaint);
}

void ImageButton::SetImageVerticalAlignment(VerticalAlignment v_alignment) {
  if (GetImageVerticalAlignment() == v_alignment)
    return;
  v_alignment_ = v_alignment;
  OnPropertyChanged(&v_alignment_, kPropertyEffectsPaint);
}

gfx::Size ImageButton::GetMinimumImageSize() const {
  return minimum_image_size_;
}

void ImageButton::SetMinimumImageSize(const gfx::Size& size) {
  if (GetMinimumImageSize() == size)
    return;
  minimum_image_size_ = size;
  OnPropertyChanged(&minimum_image_size_, kPropertyEffectsPreferredSizeChanged);
}

////////////////////////////////////////////////////////////////////////////////
// ImageButton, View overrides:

gfx::Size ImageButton::CalculatePreferredSize() const {
  gfx::Size size(kDefaultWidth, kDefaultHeight);
  if (!images_[STATE_NORMAL].isNull()) {
    size = gfx::Size(images_[STATE_NORMAL].width(),
                     images_[STATE_NORMAL].height());
  }

  size.SetToMax(GetMinimumImageSize());

  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

views::PaintInfo::ScaleType ImageButton::GetPaintScaleType() const {
  // ImageButton contains an image which is rastered at the device scale factor.
  // By default, the paint commands are recorded at a scale factor slighlty
  // different from the device scale factor. Re-rastering the image at this
  // paint recording scale will result in a distorted image. Paint recording
  // scale might also not be uniform along the x and y axis, thus resulting in
  // further distortion in the aspect ratio of the final image.
  // |kUniformScaling| ensures that the paint recording scale is uniform along
  // the x & y axis and keeps the scale equal to the device scale factor.
  // See http://crbug.com/754010 for more details.
  return views::PaintInfo::ScaleType::kUniformScaling;
}

void ImageButton::PaintButtonContents(gfx::Canvas* canvas) {
  // TODO(estade|tdanderson|bruthig): The ink drop layer should be positioned
  // behind the button's image which means the image needs to be painted to its
  // own layer instead of to the Canvas.
  gfx::ImageSkia img = GetImageToPaint();

  if (!img.isNull()) {
    gfx::ScopedCanvas scoped(canvas);
    if (draw_image_mirrored_) {
      canvas->Translate(gfx::Vector2d(width(), 0));
      canvas->Scale(-1, 1);
    }

    if (!background_image_.isNull()) {
      // The background image alignment is the same as for the image.
      gfx::Point background_position =
          ComputeImagePaintPosition(background_image_);
      canvas->DrawImageInt(background_image_, background_position.x(),
                           background_position.y());
    }

    gfx::Point position = ComputeImagePaintPosition(img);
    canvas->DrawImageInt(img, position.x(), position.y());
  }
}

////////////////////////////////////////////////////////////////////////////////
// ImageButton, protected:

gfx::ImageSkia ImageButton::GetImageToPaint() {
  gfx::ImageSkia img;

  if (!images_[STATE_HOVERED].isNull() && hover_animation().is_animating()) {
    img = gfx::ImageSkiaOperations::CreateBlendedImage(
        images_[STATE_NORMAL], images_[STATE_HOVERED],
        hover_animation().GetCurrentValue());
  } else {
    img = images_[state()];
  }

  return !img.isNull() ? img : images_[STATE_NORMAL];
}

////////////////////////////////////////////////////////////////////////////////
// ImageButton, private:

const gfx::Point ImageButton::ComputeImagePaintPosition(
    const gfx::ImageSkia& image) const {
  HorizontalAlignment h_alignment = GetImageHorizontalAlignment();
  VerticalAlignment v_alignment = GetImageVerticalAlignment();
  if (draw_image_mirrored_) {
    if (h_alignment == ALIGN_RIGHT)
      h_alignment = ALIGN_LEFT;
    else if (h_alignment == ALIGN_LEFT)
      h_alignment = ALIGN_RIGHT;
  }

  const gfx::Rect rect = GetContentsBounds();

  int x = 0;
  if (h_alignment == ALIGN_CENTER)
    x = (rect.width() - image.width()) / 2;
  else if (h_alignment == ALIGN_RIGHT)
    x = rect.width() - image.width();

  int y = 0;
  if (v_alignment == ALIGN_MIDDLE)
    y = (rect.height() - image.height()) / 2;
  else if (v_alignment == ALIGN_BOTTOM)
    y = rect.height() - image.height();

  return rect.origin() + gfx::Vector2d(x, y);
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, public:

ToggleImageButton::ToggleImageButton(ButtonListener* listener)
    : ImageButton(listener),
      toggled_(false) {
}

ToggleImageButton::~ToggleImageButton() = default;

void ToggleImageButton::SetToggled(bool toggled) {
  if (toggled == toggled_)
    return;

  for (int i = 0; i < STATE_COUNT; ++i)
    std::swap(images_[i], alternate_images_[i]);
  toggled_ = toggled;
  SchedulePaint();

  NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
}

void ToggleImageButton::SetToggledImage(ButtonState image_state,
                                        const gfx::ImageSkia* image) {
  if (toggled_) {
    images_[image_state] = image ? *image : gfx::ImageSkia();
    if (state() == image_state)
      SchedulePaint();
  } else {
    alternate_images_[image_state] = image ? *image : gfx::ImageSkia();
  }
}

void ToggleImageButton::SetToggledTooltipText(const base::string16& tooltip) {
  toggled_tooltip_text_ = tooltip;
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, ImageButton overrides:

const gfx::ImageSkia& ToggleImageButton::GetImage(
    ButtonState image_state) const {
  if (toggled_)
    return alternate_images_[image_state];
  return images_[image_state];
}

void ToggleImageButton::SetImage(ButtonState image_state,
                                 const gfx::ImageSkia& image) {
  if (toggled_) {
    alternate_images_[image_state] = image;
  } else {
    images_[image_state] = image;
    if (state() == image_state)
      SchedulePaint();
  }
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, View overrides:

base::string16 ToggleImageButton::GetTooltipText(const gfx::Point& p) const {
  return (!toggled_ || toggled_tooltip_text_.empty())
             ? Button::GetTooltipText(p)
             : toggled_tooltip_text_;
}

void ToggleImageButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ImageButton::GetAccessibleNodeData(node_data);
  node_data->SetName(GetTooltipText(gfx::Point()));

  // Use the visual pressed image as a cue for making this control into an
  // accessible toggle button.
  if ((toggled_ && !images_[ButtonState::STATE_NORMAL].isNull()) ||
      (!toggled_ && !alternate_images_[ButtonState::STATE_NORMAL].isNull())) {
    node_data->role = ax::mojom::Role::kToggleButton;
    node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  }
}

bool ToggleImageButton::toggled_for_testing() const {
  return toggled_;
}

DEFINE_ENUM_CONVERTERS(ImageButton::HorizontalAlignment,
                       {ImageButton::HorizontalAlignment::ALIGN_LEFT,
                        base::ASCIIToUTF16("ALIGN_LEFT")},
                       {ImageButton::HorizontalAlignment::ALIGN_CENTER,
                        base::ASCIIToUTF16("ALIGN_CENTER")},
                       {ImageButton::HorizontalAlignment::ALIGN_RIGHT,
                        base::ASCIIToUTF16("ALIGN_RIGHT")})
DEFINE_ENUM_CONVERTERS(ImageButton::VerticalAlignment,
                       {ImageButton::VerticalAlignment::ALIGN_TOP,
                        base::ASCIIToUTF16("ALIGN_TOP")},
                       {ImageButton::VerticalAlignment::ALIGN_MIDDLE,
                        base::ASCIIToUTF16("ALIGN_MIDDLE")},
                       {ImageButton::VerticalAlignment::ALIGN_BOTTOM,
                        base::ASCIIToUTF16("ALIGN_BOTTOM")})

BEGIN_METADATA(ImageButton)
METADATA_PARENT_CLASS(Button)
ADD_PROPERTY_METADATA(ImageButton,
                      HorizontalAlignment,
                      ImageHorizontalAlignment)
ADD_PROPERTY_METADATA(ImageButton, VerticalAlignment, ImageVerticalAlignment)
ADD_PROPERTY_METADATA(ImageButton, gfx::Size, MinimumImageSize)
END_METADATA()

}  // namespace views
