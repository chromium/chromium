// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/md_text_button_with_down_arrow.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

using base::ASCIIToUTF16;

namespace {
const char16_t kLabelButton[] = u"Label Button";
const char16_t kLongText[] =
    u"Start of Really Really Really Really Really Really "
    u"Really Really Really Really Really Really Really "
    u"Really Really Really Really Really Long Button Text";
}  // namespace

namespace views::examples {

// Creates a rounded rect with a border plus shadow. This is used by FabButton
// to draw the button background.
class SolidRoundRectPainterWithShadow : public Painter {
 public:
  SolidRoundRectPainterWithShadow(SkColor bg_color,
                                  SkColor stroke_color,
                                  float radius,
                                  const gfx::Insets& insets,
                                  SkBlendMode blend_mode,
                                  bool antialias,
                                  bool has_shadow)
      : bg_color_(bg_color),
        stroke_color_(stroke_color),
        radius_(radius),
        insets_(insets),
        blend_mode_(blend_mode),
        antialias_(antialias),
        has_shadow_(has_shadow) {}

  SolidRoundRectPainterWithShadow(const SolidRoundRectPainterWithShadow&) =
      delete;
  SolidRoundRectPainterWithShadow& operator=(
      const SolidRoundRectPainterWithShadow&) = delete;

  ~SolidRoundRectPainterWithShadow() override = default;

  // Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();
    float scaled_radius = radius_ * scale;

    gfx::Rect inset_rect(size);
    inset_rect.Inset(insets_);
    cc::PaintFlags flags;
    // Draw a shadow effect by shrinking the rect and then inserting a
    // shadow looper.
    if (has_shadow_) {
      gfx::Rect shadow_bounds = inset_rect;
      gfx::ShadowValues shadow;
      constexpr int kOffset = 2;
      constexpr int kBlur = 4;
      shadow.emplace_back(gfx::Vector2d(kOffset, kOffset), kBlur,
                          SkColorSetA(SK_ColorBLACK, 0x24));
      shadow_bounds.Inset(-gfx::ShadowValue::GetMargin(shadow));
      inset_rect.Inset(-gfx::ShadowValue::GetMargin(shadow));
      flags.setAntiAlias(true);
      flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
      canvas->DrawRoundRect(shadow_bounds, scaled_radius, flags);
    }

    gfx::RectF fill_rect(gfx::ScaleToEnclosingRect(inset_rect, scale));
    gfx::RectF stroke_rect = fill_rect;

    flags.setBlendMode(blend_mode_);
    flags.setAntiAlias(antialias_);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(bg_color_);
    canvas->DrawRoundRect(fill_rect, scaled_radius, flags);

    if (stroke_color_ != SK_ColorTRANSPARENT && !has_shadow_) {
      constexpr float kStrokeWidth = 1.0f;
      stroke_rect.Inset(gfx::InsetsF(kStrokeWidth / 2));
      scaled_radius -= kStrokeWidth / 2;
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kStrokeWidth);
      flags.setColor(stroke_color_);
      canvas->DrawRoundRect(stroke_rect, scaled_radius, flags);
    }
  }

 private:
  const SkColor bg_color_;
  const SkColor stroke_color_;
  const float radius_;
  const gfx::Insets insets_;
  const SkBlendMode blend_mode_;
  const bool antialias_;
  const bool has_shadow_;
};

// Floating Action Button (Fab) is a button that has a shadow around the button
// to simulate a floating effect. This class is not used officially in the Views
// library. This is a prototype of a potential way to implement such an effect
// by overriding the hover effect to draw a new background with a shadow.
class FabButton : public views::MdTextButton {
  METADATA_HEADER(FabButton, views::MdTextButton)

 public:
  using MdTextButton::MdTextButton;
  FabButton(const FabButton&) = delete;
  FabButton& operator=(const FabButton&) = delete;
  ~FabButton() override = default;

  void UpdateBackgroundColor() override {
    SkColor bg_color = GetColorProvider()->GetColor(
        ExamplesColorIds::kColorButtonBackgroundFab);
    SetBackground(CreateBackgroundFromPainter(
        std::make_unique<SolidRoundRectPainterWithShadow>(
            bg_color, SK_ColorTRANSPARENT, GetCornerRadiusValue(),
            gfx::Insets(), SkBlendMode::kSrcOver, true, use_shadow_)));
  }

  void OnHoverChanged() {
    use_shadow_ = !use_shadow_;
    UpdateBackgroundColor();
  }

  void OnThemeChanged() override {
    MdTextButton::OnThemeChanged();
    UpdateBackgroundColor();
  }

 private:
  base::CallbackListSubscription highlighted_changed_subscription_ =
      InkDrop::Get(this)->AddHighlightedChangedCallback(
          base::BindRepeating([](FabButton* host) { host->OnHoverChanged(); },
                              base::Unretained(this)));
  bool use_shadow_ = false;
};

BEGIN_METADATA(FabButton)
END_METADATA

ButtonExample::ButtonExample() : ExampleBase("Button") {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_ = rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia();
}

ButtonExample::~ButtonExample() = default;

void ButtonExample::CreateExampleView(View* container) {
  container->SetUseDefaultFillLayout(true);

  auto view = Builder<BoxLayoutView>()
                  .SetOrientation(BoxLayout::Orientation::kVertical)
                  .SetInsideBorderInsets(gfx::Insets(10))
                  .SetBetweenChildSpacing(10)
                  .SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kCenter)
                  .AddChildren(Builder<LabelButton>()
                                   .CopyAddressTo(&label_button_)
                                   .SetText(kLabelButton)
                                   .SetRequestFocusOnPress(true)
                                   .SetCallback(base::BindRepeating(
                                       &ButtonExample::LabelButtonPressed,
                                       base::Unretained(this), label_button_)),
                               Builder<MdTextButton>()
                                   .CopyAddressTo(&md_button_)
                                   .SetText(u"Material Design"),
                               Builder<MdTextButton>()
                                   .CopyAddressTo(&md_disabled_button_)
                                   .SetText(u"Material Design Disabled Button")
                                   .SetState(Button::STATE_DISABLED),
                               Builder<MdTextButton>()
                                   .CopyAddressTo(&md_default_button_)
                                   .SetText(u"Default")
                                   .SetIsDefault(true),
                               Builder<MdTextButton>()
                                   .CopyAddressTo(&md_tonal_button_)
                                   .SetStyle(ui::ButtonStyle::kTonal)
                                   .SetText(u"Tonal"),
                               Builder<MdTextButton>()
                                   .CopyAddressTo(&md_text_button_)
                                   .SetStyle(ui::ButtonStyle::kText)
                                   .SetText(u"Material Text"),
                               Builder<MdTextButton>()
                                   .CopyAddressTo(&md_icon_text_button_)
                                   .SetText(u"Material Text with Icon"),
                               Builder<ImageButton>()
                                   .CopyAddressTo(&image_button_)
                                   .SetAccessibleName(l10n_util::GetStringUTF16(
                                       IDS_BUTTON_IMAGE_BUTTON_AX_LABEL))
                                   .SetRequestFocusOnPress(true)
                                   .SetCallback(base::BindRepeating(
                                       &ButtonExample::ImageButtonPressed,
                                       base::Unretained(this))))
                  .Build();
  md_icon_text_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(views::kInfoIcon));
  view->AddChildView(std::make_unique<FabButton>(
      base::BindRepeating(&ButtonExample::ImageButtonPressed,
                          base::Unretained(this)),
      u"Fab Prototype"));

  view->AddChildView(ImageButton::CreateIconButton(
      base::BindRepeating(&ButtonExample::ImageButtonPressed,
                          base::Unretained(this)),
      views::kLaunchIcon, u"Icon button"));
  view->AddChildView(std::make_unique<views::MdTextButtonWithDownArrow>(
      base::BindRepeating(&ButtonExample::ImageButtonPressed,
                          base::Unretained(this)),
      u"TextButton with down arrow"));

  image_button_->SetImageModel(ImageButton::STATE_NORMAL,
                               ui::ImageModel::FromResourceId(IDR_CLOSE));
  image_button_->SetImageModel(ImageButton::STATE_HOVERED,
                               ui::ImageModel::FromResourceId(IDR_CLOSE_H));
  image_button_->SetImageModel(ImageButton::STATE_PRESSED,
                               ui::ImageModel::FromResourceId(IDR_CLOSE_P));

  container->AddChildView(std::move(view));
}

void ButtonExample::LabelButtonPressed(LabelButton* label_button,
                                       const ui::Event& event) {
  PrintStatus("Label Button Pressed! count: %d", ++count_);
  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      label_button->SetText(
          label_button->GetText().empty()
              ? kLongText
              : label_button->GetText().length() > 50 ? kLabelButton : u"");
    } else if (event.IsAltDown()) {
      label_button->SetImageModel(
          Button::STATE_NORMAL,
          label_button->GetImage(Button::STATE_NORMAL).isNull()
              ? ui::ImageModel::FromImageSkia(*icon_)
              : ui::ImageModel());
    } else {
      static int alignment = 0;
      label_button->SetHorizontalAlignment(
          static_cast<gfx::HorizontalAlignment>(++alignment % 3));
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      // Toggle focusability.
      label_button->GetViewAccessibility().IsAccessibilityFocusable()
          ? label_button->SetFocusBehavior(View::FocusBehavior::NEVER)
          : label_button->SetFocusBehavior(
                PlatformStyle::kDefaultFocusBehavior);
    }
  } else if (event.IsAltDown()) {
    label_button->SetIsDefault(!label_button->GetIsDefault());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
  PrintViewHierarchy(example_view());
}

void ButtonExample::ImageButtonPressed() {
  PrintStatus("Image Button Pressed! count: %d", ++count_);
}

}  // namespace views::examples
