// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_view_linux.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/shadow_value.h"
#include "ui/linux/linux_ui.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/frame_view_layout_linux.h"
#include "ui/views/window/frame_view_utils_linux.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace views {

namespace {

// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
constexpr int kResizeAreaCornerSize = 16;

// The resize border at the top of the caption area, used when frame shadows
// are not drawn. Matches the visible frame border on the other three edges.
constexpr int kResizeTopBorderThickness = 4;

}  // namespace

FrameViewLinux::FrameViewLinux(Widget* widget, FrameViewLayoutLinux* layout)
    : widget_(widget), frame_background_(std::make_unique<FrameBackground>()) {
  if (!layout) {
    layout = new FrameViewLayoutLinux();
  }
  layout->set_view(this);
  layout_ = SetLayoutManager(std::unique_ptr<FrameViewLayoutLinux>(layout));

  paint_as_active_subscription_ =
      widget_->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &FrameViewLinux::OnPaintAsActiveChanged, base::Unretained(this)));
}

void FrameViewLinux::InitViews() {
  CreateCaptionButtons();

  if (auto* linux_ui = ui::LinuxUi::instance()) {
    button_order_observation_.Observe(linux_ui);
    OnWindowButtonOrderingChange();
  }
}

void FrameViewLinux::CreateCaptionButtons() {
  auto create_button =
      [this](Button::PressedCallback callback, CaptionButtonIcon icon,
             int hit_test_type, const gfx::VectorIcon& image,
             int accessibility_string_id) -> FrameCaptionButton* {
    auto* button = AddChildView(std::make_unique<FrameCaptionButton>(
        std::move(callback), icon, hit_test_type));
    button->SetImage(icon, FrameCaptionButton::Animate::kNo, image);
    button->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(accessibility_string_id));
    return button;
  };

  close_button_ = create_button(
      base::BindRepeating(&Widget::CloseWithReason, base::Unretained(widget_),
                          Widget::ClosedReason::kCloseButtonClicked),
      CAPTION_BUTTON_ICON_CLOSE, HTCLOSE, kWindowControlCloseOldIcon,
      IDS_APP_ACCNAME_CLOSE);

  minimize_button_ = create_button(
      base::BindRepeating(&Widget::Minimize, base::Unretained(widget_)),
      CAPTION_BUTTON_ICON_MINIMIZE, HTMINBUTTON, kWindowControlMinimizeOldIcon,
      IDS_APP_ACCNAME_MINIMIZE);

  maximize_button_ = create_button(
      base::BindRepeating(&Widget::Maximize, base::Unretained(widget_)),
      CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, HTMAXBUTTON,
      kWindowControlMaximizeOldIcon, IDS_APP_ACCNAME_MAXIMIZE);

  restore_button_ = create_button(
      base::BindRepeating(&Widget::Restore, base::Unretained(widget_)),
      CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, HTMAXBUTTON,
      kWindowControlRestoreOldIcon, IDS_APP_ACCNAME_RESTORE);
}

FrameViewLinux::~FrameViewLinux() {
  layout_->set_view(nullptr);
}

gfx::Rect FrameViewLinux::GetBoundsForClientView() const {
  return layout_->GetBoundsForClientView();
}

gfx::Rect FrameViewLinux::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return layout_->GetWindowBoundsForClientBounds(client_bounds);
}

int FrameViewLinux::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point)) {
    return HTNOWHERE;
  }

  int frame_component = widget_->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE) {
    return frame_component;
  }

  using Type = ui::NavButtonProvider::FrameButtonDisplayType;
  auto check_button = [&](Type type, int hit_code) -> int {
    auto* button = GetButtonFromType(type);
    if (button && button->GetVisible() &&
        button->GetMirroredBounds().Contains(point)) {
      return hit_code;
    }
    return HTNOWHERE;
  };

  int result = check_button(Type::kClose, HTCLOSE);
  if (result != HTNOWHERE) {
    return result;
  }

  result = check_button(Type::kRestore, HTMAXBUTTON);
  if (result != HTNOWHERE) {
    return result;
  }

  result = check_button(Type::kMaximize, HTMAXBUTTON);
  if (result != HTNOWHERE) {
    return result;
  }

  result = check_button(Type::kMinimize, HTMINBUTTON);
  if (result != HTNOWHERE) {
    return result;
  }

  gfx::Insets resize_border = GetFrameBorderInsets();
  int window_component = GetHTComponentForFrame(
      point, resize_border, kResizeAreaCornerSize, kResizeAreaCornerSize,
      widget_->widget_delegate()->CanResize());

  if (window_component != HTNOWHERE) {
    return window_component;
  }

  if (!ShouldDrawRestoredFrameShadow() && !widget_->IsMaximized() &&
      !widget_->IsFullscreen() && widget_->widget_delegate()->CanResize() &&
      point.y() < kResizeTopBorderThickness) {
    return HTTOP;
  }

  return HTCAPTION;
}

void FrameViewLinux::GetWindowMask(const gfx::Size& size, SkPath* window_mask) {
  // This class uses transparency to draw rounded corners, so a
  // window mask is not necessary.
}

void FrameViewLinux::ResetWindowControls() {
  using Type = ui::NavButtonProvider::FrameButtonDisplayType;
  for (auto type : {Type::kMinimize, Type::kMaximize, Type::kRestore}) {
    auto* button = GetButtonFromType(type);
    if (button) {
      button->SetState(Button::STATE_NORMAL);
    }
  }
}

bool FrameViewLinux::HasWindowTitle() const {
  return widget_->widget_delegate()->ShouldShowWindowTitle();
}

bool FrameViewLinux::IsWindowTitleVisible() const {
  return HasWindowTitle() && !title_bounds_.IsEmpty();
}

void FrameViewLinux::UpdateWindowTitle() {
  if (HasWindowTitle()) {
    SchedulePaintInRect(title_bounds_);
  }
}

void FrameViewLinux::SizeConstraintsChanged() {
  ResetWindowControls();
  InvalidateLayout();
}

void FrameViewLinux::OnPaint(gfx::Canvas* canvas) {
  if (!layout_->ShouldShowTitlebarAndBorder()) {
    return;
  }

  const bool active = ShouldPaintAsActive();
  const auto* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  SkColor frame_color = color_provider->GetColor(
      active ? ui::kColorFrameActive : ui::kColorFrameInactive);

  frame_background_->set_frame_color(frame_color);
  frame_background_->set_is_active(active);
  frame_background_->set_top_area_height(layout_->GetTopAreaHeight());

  UpdateButtonColors();

  if (widget_->IsMaximized()) {
    PaintMaximizedFrameBorder(canvas);
  } else {
    PaintRestoredFrameBorder(canvas);
  }

  PaintTitlebar(canvas);
}

void FrameViewLinux::Layout(PassKey) {
  LayoutSuperclass<FrameView>(this);
}

gfx::Size FrameViewLinux::GetMaximumSize() const {
  gfx::Size max_size = widget_->client_view()->GetMaximumSize();
  gfx::Size converted_size =
      widget_->non_client_view()
          ->GetWindowBoundsForClientBounds(gfx::Rect(max_size))
          .size();
  return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                   max_size.height() == 0 ? 0 : converted_size.height());
}

gfx::Insets FrameViewLinux::GetFrameBorderInsets() const {
  return layout_->GetFrameBorderInsets();
}

gfx::Insets FrameViewLinux::GetRestoredFrameBorderInsets() const {
  return layout_->GetRestoredFrameBorderInsets();
}

SkRRect FrameViewLinux::GetRestoredClipRegion() const {
  gfx::InsetsF border = ShouldDrawRestoredFrameShadow()
                            ? gfx::InsetsF(GetRestoredFrameBorderInsets())
                            : gfx::InsetsF();
  return views::GetRestoredClipRegion(gfx::RectF(GetLocalBounds()), border,
                                      GetCornerRadii());
}

void FrameViewLinux::SetSupportsClientFrameShadow(bool supports) {
  layout_->set_supports_client_frame_shadow(supports);
}

bool FrameViewLinux::ShouldDrawRestoredFrameShadow() const {
  return layout_->supports_client_frame_shadow();
}

void FrameViewLinux::SetTiled(bool tiled) {
  if (layout_->tiled() == tiled) {
    return;
  }
  layout_->set_tiled(tiled);
  OnThemeOrButtonOrderChanged();
}

gfx::Insets FrameViewLinux::GetInputInsets() const {
  return layout_->GetInputInsets();
}

int FrameViewLinux::GetTranslucentTopAreaHeight() const {
  return layout_->GetTranslucentTopAreaHeight();
}

gfx::RoundedCornersF FrameViewLinux::GetCornerRadii() const {
  return layout_->GetCornerRadii();
}

gfx::ShadowValues FrameViewLinux::GetShadowValues(bool active) const {
  return layout_->GetShadowValues(active);
}

Button* FrameViewLinux::GetFrameButton(FrameButton frame_button) {
  auto display_type = GetButtonDisplayType(frame_button);
  return GetButtonFromType(display_type);
}

Button* FrameViewLinux::GetButtonFromType(
    ui::NavButtonProvider::FrameButtonDisplayType type) {
  switch (type) {
    case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
      return minimize_button_;
    case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      return maximize_button_;
    case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
      return restore_button_;
    case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
      return close_button_;
  }
}

ui::NavButtonProvider::FrameButtonDisplayType
FrameViewLinux::GetButtonDisplayType(FrameButton button_id) const {
  return GetFrameButtonDisplayType(button_id, widget_->IsMaximized());
}

bool FrameViewLinux::IsTiled() const {
  return layout_->tiled();
}

void FrameViewLinux::OnThemeOrButtonOrderChanged() {
  InvalidateLayout();
  SchedulePaint();
}

void FrameViewLinux::PaintRestoredFrameBorder(gfx::Canvas* canvas) {
  bool showing_shadow = ShouldDrawRestoredFrameShadow();
  auto shadow_values = showing_shadow ? GetShadowValues(ShouldPaintAsActive())
                                      : gfx::ShadowValues();
  gfx::Insets border = GetFrameBorderInsets();

  PaintRestoredFrameBorderLinux(
      *canvas, *this, frame_background_.get(), GetRestoredClipRegion(),
      showing_shadow, ShouldPaintAsActive(), border, shadow_values, IsTiled());
}

void FrameViewLinux::PaintMaximizedFrameBorder(gfx::Canvas* canvas) {
  frame_background_->PaintMaximized(canvas, this);
}

void FrameViewLinux::OnThemeChanged() {
  FrameView::OnThemeChanged();
  OnThemeOrButtonOrderChanged();
}

void FrameViewLinux::OnWindowButtonOrderingChange() {
  OnThemeOrButtonOrderChanged();
}

void FrameViewLinux::OnPaintAsActiveChanged() {
  SchedulePaint();
}

void FrameViewLinux::PaintTitlebar(gfx::Canvas* canvas) {
  if (!widget_->widget_delegate() ||
      !widget_->widget_delegate()->ShouldShowWindowTitle()) {
    return;
  }
  const auto* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }
  gfx::Rect rect = title_bounds_;
  rect.set_x(GetMirroredXForRect(title_bounds_));
  bool active = ShouldPaintAsActive();
  canvas->DrawStringRectWithFlags(
      widget_->widget_delegate()->GetWindowTitle(), layout_->GetTitleFontList(),
      color_provider->GetColor(active
                                   ? ui::kColorFrameCaptionForegroundActive
                                   : ui::kColorFrameCaptionForegroundInactive),
      rect, gfx::Canvas::TEXT_ALIGN_CENTER);
}

void FrameViewLinux::UpdateButtonColors() {
  const auto* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }
  bool active = ShouldPaintAsActive();
  SkColor frame_color = color_provider->GetColor(
      active ? ui::kColorFrameActive : ui::kColorFrameInactive);
  for (Button* button : {minimize_button_.get(), maximize_button_.get(),
                         restore_button_.get(), close_button_.get()}) {
    if (button) {
      auto* frame_caption_button = static_cast<FrameCaptionButton*>(button);
      frame_caption_button->SetBackgroundColor(frame_color);
      frame_caption_button->SetPaintAsActive(active);
    }
  }
}

BEGIN_METADATA(FrameViewLinux)
END_METADATA

}  // namespace views
