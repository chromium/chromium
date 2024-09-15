// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_button.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
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

ImageButton::ImageButton(PressedCallback callback)
    : Button(std::move(callback)) {
  // By default, we request that the gfx::Canvas passed to our View::OnPaint()
  // implementation is flipped horizontally so that the button's images are
  // mirrored when the UI directionality is right-to-left.
  SetFlipCanvasOnPaintForRTLUI(true);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
}

ImageButton::~ImageButton() = default;

gfx::ImageSkia ImageButton::GetImage(ButtonState state) const {
  return images_[state].Rasterize(GetColorProvider());
}

void ImageButton::SetImageModel(ButtonState for_state,
                                const ui::ImageModel& image_model) {
  if (for_state == STATE_HOVERED)
    SetAnimateOnStateChange(!image_model.IsEmpty());
  const gfx::Size old_preferred_size = GetPreferredSize({});
  images_[for_state] = image_model;

  if (old_preferred_size != GetPreferredSize({})) {
    PreferredSizeChanged();
  }

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

  background_image_ =
      gfx::ImageSkiaOperations::CreateButtonBackground(color, *image, *mask);
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

gfx::Size ImageButton::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  gfx::Size size(kDefaultWidth, kDefaultHeight);
  if (!images_[STATE_NORMAL].IsEmpty())
    size = images_[STATE_NORMAL].Size();

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

void ImageButton::OnThemeChanged() {
  Button::OnThemeChanged();

  // If we have any `ImageModel`s, they may need repaint upon a `ColorProvider`
  // change.
  SchedulePaint();
}

// static
std::unique_ptr<ImageButton> ImageButton::CreateIconButton(
    PressedCallback callback,
    const gfx::VectorIcon& icon,
    const std::u16string& accessible_name,
    MaterialIconStyle icon_style,
    std::optional<gfx::Insets> insets) {
  const int kSmallIconSize = 16;
  const int kLargeIconSize = 20;
  int icon_size = (icon_style == MaterialIconStyle::kLarge) ? kLargeIconSize
                                                            : kSmallIconSize;
  std::unique_ptr<ImageButton> icon_button =
      std::make_unique<ImageButton>(std::move(callback));
  icon_button->SetImageModel(
      ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, icon_size));
  icon_button->SetImageModel(
      ButtonState::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, icon_size));
  icon_button->SetImageModel(
      ButtonState::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, icon_size));
  icon_button->SetImageModel(
      ButtonState::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIconDisabled, icon_size));

  const gfx::Insets target_insets =
      insets.has_value() ? insets.value()
                         : LayoutProvider::Get()->GetInsetsMetric(
                               InsetsMetric::INSETS_ICON_BUTTON);
  icon_button->SetBorder(views::CreateEmptyBorder(target_insets));

  const int kSmallIconButtonSize = 24;
  const int kLargeIconButtonSize = 28;
  int button_size = (icon_style == MaterialIconStyle::kLarge)
                        ? kLargeIconButtonSize
                        : kSmallIconButtonSize;
  const int highlight_radius = LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, gfx::Size(button_size, button_size));
  views::InstallRoundRectHighlightPathGenerator(
      icon_button.get(), gfx::Insets(), highlight_radius);

  InkDrop::Get(icon_button.get())->SetMode(views::InkDropHost::InkDropMode::ON);
  icon_button->SetHasInkDropActionOnClick(true);
  icon_button->SetShowInkDropWhenHotTracked(true);
  InkDrop::Get(icon_button.get())
      ->SetBaseColorCallback(base::BindRepeating(
          [](ImageButton* host) {
            return host->GetColorProvider()->GetColor(
                ui::kColorSysOnSurfaceSubtle);
          },
          icon_button.get()));

  icon_button->GetViewAccessibility().SetName(accessible_name);
  icon_button->SetTooltipText(accessible_name);

  return icon_button;
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
  const auto* const color_provider = GetColorProvider();
  if (!images_[STATE_HOVERED].IsEmpty() && hover_animation().is_animating()) {
    return gfx::ImageSkiaOperations::CreateBlendedImage(
        images_[STATE_NORMAL].Rasterize(color_provider),
        images_[STATE_HOVERED].Rasterize(color_provider),
        hover_animation().GetCurrentValue());
  }

  const auto img = images_[GetState()].Rasterize(color_provider);
  return !img.isNull() ? img : images_[STATE_NORMAL].Rasterize(color_provider);
}

void ToggleImageButton::UpdateAccessibleRoleIfNeeded() {
  if ((toggled_ && !images_[ButtonState::STATE_NORMAL].IsEmpty()) ||
      (!toggled_ && !alternate_images_[ButtonState::STATE_NORMAL].IsEmpty())) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    return;
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
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

ToggleImageButton::ToggleImageButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  UpdateAccessibleCheckedState();
}

ToggleImageButton::~ToggleImageButton() = default;

bool ToggleImageButton::GetToggled() const {
  return toggled_;
}

void ToggleImageButton::UpdateAccessibleCheckedState() {
  // Use the visual pressed image as a cue for making this control into an
  // accessible toggle button.
  if ((toggled_ && !images_[ButtonState::STATE_NORMAL].IsEmpty()) ||
      (!toggled_ && !alternate_images_[ButtonState::STATE_NORMAL].IsEmpty())) {
    GetViewAccessibility().SetCheckedState(
        toggled_ ? ax::mojom::CheckedState::kTrue
                 : ax::mojom::CheckedState::kFalse);
  } else {
    GetViewAccessibility().RemoveCheckedState();
  }
}

void ToggleImageButton::SetToggled(bool toggled) {
  if (toggled == toggled_)
    return;

  for (size_t i = 0; i < STATE_COUNT; ++i) {
    std::swap(images_[i], alternate_images_[i]);
  }
  toggled_ = toggled;

  UpdateAccessibleCheckedState();
  OnPropertyChanged(&toggled_, kPropertyEffectsPaint);
  UpdateAccessibleRoleIfNeeded();
  UpdateAccessibleName();
}

void ToggleImageButton::SetToggledImage(ButtonState image_state,
                                        const gfx::ImageSkia* image) {
  SetToggledImageModel(image_state, ui::ImageModel::FromImageSkia(
                                        image ? *image : gfx::ImageSkia()));
}

void ToggleImageButton::SetToggledImageModel(
    ButtonState image_state,
    const ui::ImageModel& image_model) {
  if (toggled_) {
    images_[image_state] = image_model;
    if (GetState() == image_state)
      SchedulePaint();
  } else {
    alternate_images_[image_state] = image_model;
  }
  UpdateAccessibleCheckedState();
  UpdateAccessibleRoleIfNeeded();
}

void ToggleImageButton::SetToggledBackground(std::unique_ptr<Background> b) {
  toggled_background_ = std::move(b);
  SchedulePaint();
}

std::u16string ToggleImageButton::GetToggledTooltipText() const {
  return toggled_tooltip_text_;
}

void ToggleImageButton::SetToggledTooltipText(const std::u16string& tooltip) {
  if (tooltip == toggled_tooltip_text_)
    return;
  toggled_tooltip_text_ = tooltip;
  UpdateAccessibleName();
  OnPropertyChanged(&toggled_tooltip_text_, kPropertyEffectsNone);
}

std::u16string ToggleImageButton::GetToggledAccessibleName() const {
  return toggled_accessible_name_;
}

void ToggleImageButton::SetToggledAccessibleName(const std::u16string& name) {
  if (name == toggled_accessible_name_)
    return;
  toggled_accessible_name_ = name;
  UpdateAccessibleName();
  OnPropertyChanged(&toggled_accessible_name_, kPropertyEffectsNone);
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, ImageButton overrides:

gfx::ImageSkia ToggleImageButton::GetImage(ButtonState image_state) const {
  if (toggled_)
    return alternate_images_[image_state].Rasterize(GetColorProvider());
  return images_[image_state].Rasterize(GetColorProvider());
}

void ToggleImageButton::SetImageModel(ButtonState image_state,
                                      const ui::ImageModel& image_model) {
  if (toggled_) {
    alternate_images_[image_state] = image_model;
  } else {
    images_[image_state] = image_model;
    if (GetState() == image_state)
      SchedulePaint();
  }
  PreferredSizeChanged();
  UpdateAccessibleCheckedState();
  UpdateAccessibleRoleIfNeeded();
}

void ToggleImageButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (toggled_ && toggled_background_) {
    TRACE_EVENT0("views", "View::OnPaintBackground");
    toggled_background_->Paint(canvas, this);
  } else {
    ImageButton::OnPaintBackground(canvas);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, View overrides:

std::u16string ToggleImageButton::GetTooltipText(const gfx::Point& p) const {
  return (!toggled_ || toggled_tooltip_text_.empty())
             ? Button::GetTooltipText(p)
             : toggled_tooltip_text_;
}

void ToggleImageButton::UpdateAccessibleName() {
  if (toggled_) {
    if (!toggled_accessible_name_.empty()) {
      GetViewAccessibility().SetName(toggled_accessible_name_);
    } else if (!toggled_tooltip_text_.empty()) {
      GetViewAccessibility().SetName(toggled_tooltip_text_);
    }
  } else {
    GetViewAccessibility().SetName(Button::GetTooltipText());
  }
}

BEGIN_METADATA(ImageButton)
ADD_PROPERTY_METADATA(HorizontalAlignment, ImageHorizontalAlignment)
ADD_PROPERTY_METADATA(VerticalAlignment, ImageVerticalAlignment)
ADD_PROPERTY_METADATA(gfx::Size, MinimumImageSize)
END_METADATA

BEGIN_METADATA(ToggleImageButton)
ADD_PROPERTY_METADATA(bool, Toggled)
ADD_PROPERTY_METADATA(std::unique_ptr<Background>, ToggledBackground)
ADD_PROPERTY_METADATA(std::u16string, ToggledTooltipText)
ADD_PROPERTY_METADATA(std::u16string, ToggledAccessibleName)
END_METADATA

}  // namespace views

DEFINE_ENUM_CONVERTERS(
    views::ImageButton::HorizontalAlignment,
    {views::ImageButton::HorizontalAlignment::ALIGN_LEFT, u"ALIGN_LEFT"},
    {views::ImageButton::HorizontalAlignment::ALIGN_CENTER, u"ALIGN_CENTER"},
    {views::ImageButton::HorizontalAlignment::ALIGN_RIGHT, u"ALIGN_RIGHT"})
DEFINE_ENUM_CONVERTERS(
    views::ImageButton::VerticalAlignment,
    {views::ImageButton::VerticalAlignment::ALIGN_TOP, u"ALIGN_TOP"},
    {views::ImageButton::VerticalAlignment::ALIGN_MIDDLE, u"ALIGN_MIDDLE"},
    {views::ImageButton::VerticalAlignment::ALIGN_BOTTOM, u"ALIGN_BOTTOM"})
