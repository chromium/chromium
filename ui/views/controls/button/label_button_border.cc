// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_border.h"

#include <utility>

#include "cc/paint/paint_flags.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/native_theme_delegate.h"
#include "ui/views/resources/grit/views_resources.h"

namespace views {

namespace {

// The text-button hot and pushed image IDs; normal is unadorned by default.
constexpr int kTextHoveredImages[] = IMAGE_GRID(IDR_TEXTBUTTON_HOVER);
constexpr int kTextPressedImages[] = IMAGE_GRID(IDR_TEXTBUTTON_PRESSED);

// A helper function to paint the appropriate broder images.
void PaintHelper(LabelButtonAssetBorder* border,
                 gfx::Canvas* canvas,
                 ui::NativeTheme::State state,
                 const gfx::Rect& rect,
                 const ui::NativeTheme::ExtraParams& extra) {
  const auto& button = absl::get<ui::NativeTheme::ButtonExtraParams>(extra);
  Painter* painter =
      border->GetPainter(button.is_focused, Button::GetButtonStateFrom(state));
  // Paint any corresponding unfocused painter if there is no focused painter.
  if (!painter && button.is_focused) {
    painter = border->GetPainter(false, Button::GetButtonStateFrom(state));
  }
  if (painter)
    Painter::PaintPainterAt(canvas, painter, rect);
}

}  // namespace

LabelButtonBorder::LabelButtonBorder() = default;
LabelButtonBorder::~LabelButtonBorder() = default;

bool LabelButtonBorder::PaintsButtonState(bool focused,
                                          Button::ButtonState state) {
  return false;
}

void LabelButtonBorder::Paint(const View& view, gfx::Canvas* canvas) {}

gfx::Insets LabelButtonBorder::GetInsets() const {
  return insets_;
}

gfx::Size LabelButtonBorder::GetMinimumSize() const {
  return gfx::Size();
}

LabelButtonAssetBorder::LabelButtonAssetBorder() {
  set_insets(GetDefaultInsets());

  SetPainter(false, Button::STATE_HOVERED,
             Painter::CreateImageGridPainter(kTextHoveredImages));
  SetPainter(false, Button::STATE_PRESSED,
             Painter::CreateImageGridPainter(kTextPressedImages));
}

LabelButtonAssetBorder::~LabelButtonAssetBorder() = default;

// static
gfx::Insets LabelButtonAssetBorder::GetDefaultInsets() {
  return LayoutProvider::Get()->GetInsetsMetric(
      InsetsMetric::INSETS_LABEL_BUTTON);
}

bool LabelButtonAssetBorder::PaintsButtonState(bool focused,
                                               Button::ButtonState state) {
  // PaintHelper() above will paint the unfocused painter for a given state if
  // the button is focused, but there is no focused painter.
  return GetPainter(focused, state) || (focused && GetPainter(false, state));
}

void LabelButtonAssetBorder::Paint(const View& view, gfx::Canvas* canvas) {
  const NativeThemeDelegate* native_theme_delegate =
      static_cast<const LabelButton*>(&view);
  gfx::Rect rect(native_theme_delegate->GetThemePaintRect());
  ui::NativeTheme::ExtraParams extra(
      absl::in_place_type<ui::NativeTheme::ButtonExtraParams>);
  const gfx::Animation* animation = native_theme_delegate->GetThemeAnimation();
  ui::NativeTheme::State state = native_theme_delegate->GetThemeState(&extra);

  if (animation && animation->is_animating()) {
    // Linearly interpolate background and foreground painters during animation.
    uint8_t fg_alpha =
        static_cast<uint8_t>(animation->CurrentValueBetween(0, 255));

    const SkRect sk_rect = gfx::RectToSkRect(rect);
    cc::PaintCanvasAutoRestore auto_restore(canvas->sk_canvas(), false);
    canvas->sk_canvas()->saveLayer(sk_rect, cc::PaintFlags());

    {
      // First, modulate the background by 1 - alpha.
      cc::PaintCanvasAutoRestore auto_restore_alpha(canvas->sk_canvas(), false);
      canvas->SaveLayerAlpha(255 - fg_alpha, rect);
      state = native_theme_delegate->GetBackgroundThemeState(&extra);
      PaintHelper(this, canvas, state, rect, extra);
    }

    // Then modulate the foreground by alpha, and blend using kPlus_Mode.
    cc::PaintFlags flags;
    flags.setAlphaf(fg_alpha / 255.0f);
    flags.setBlendMode(SkBlendMode::kPlus);
    canvas->sk_canvas()->saveLayer(sk_rect, flags);
    state = native_theme_delegate->GetForegroundThemeState(&extra);
    PaintHelper(this, canvas, state, rect, extra);
  } else {
    PaintHelper(this, canvas, state, rect, extra);
  }
}

gfx::Size LabelButtonAssetBorder::GetMinimumSize() const {
  gfx::Size minimum_size;
  for (const auto& painters_for_focus_state : painters_) {
    for (const auto& painter_for_button_state : painters_for_focus_state) {
      if (painter_for_button_state)
        minimum_size.SetToMax(painter_for_button_state->GetMinimumSize());
    }
  }
  return minimum_size;
}

Painter* LabelButtonAssetBorder::GetPainter(bool focused,
                                            Button::ButtonState state) {
  return painters_[focused ? 1 : 0][state].get();
}

void LabelButtonAssetBorder::SetPainter(bool focused,
                                        Button::ButtonState state,
                                        std::unique_ptr<Painter> painter) {
  painters_[focused ? 1 : 0][state] = std::move(painter);
}

}  // namespace views
