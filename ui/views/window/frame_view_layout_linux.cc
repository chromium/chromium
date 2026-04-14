// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_view_layout_linux.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_buttons.h"
#include "ui/views/window/frame_view_linux.h"
#include "ui/views/window/frame_view_utils_linux.h"
#include "ui/views/window/window_button_order_provider.h"

namespace views {

namespace {

// The space between the title text and the caption buttons.
constexpr int kTitleCaptionSpacing = 5;

// Default vertical padding around the title text.
constexpr int kDefaultTitlebarVerticalPadding = 4;

// Default visible frame border thickness when window shadow is not drawn.
constexpr int kDefaultFrameBorderThickness = 4;

void SetButtonVisible(Button* button, const gfx::Rect& bounds) {
  button->SetVisible(true);
  button->SetBoundsRect(bounds);
}

}  // namespace

FrameViewLayoutLinux::FrameViewLayoutLinux() = default;

FrameViewLayoutLinux::~FrameViewLayoutLinux() = default;

gfx::FontList FrameViewLayoutLinux::GetTitleFontList() const {
  return TypographyProvider::Get().GetWindowTitleFontList();
}

gfx::ShadowValues FrameViewLayoutLinux::GetShadowValues(bool active) const {
  Widget* widget = view_->GetWidget();
  if (widget->IsMaximized() || widget->IsFullscreen() || tiled_) {
    return gfx::ShadowValues();
  }
  auto* provider = LayoutProvider::Get();
  if (!provider) {
    return gfx::ShadowValues();
  }
  return gfx::ShadowValue::MakeMdShadowValues(
      provider->GetShadowElevationMetric(active ? Emphasis::kMaximum
                                                : Emphasis::kMedium));
}

gfx::RoundedCornersF FrameViewLayoutLinux::GetCornerRadii() const {
  if (!supports_client_frame_shadow_ || tiled_) {
    return gfx::RoundedCornersF();
  }
  auto* provider = LayoutProvider::Get();
  if (!provider) {
    return gfx::RoundedCornersF();
  }
  float radius =
      static_cast<float>(provider->GetCornerRadiusMetric(Emphasis::kHigh));
  return gfx::RoundedCornersF(radius, radius, 0, 0);
}

void FrameViewLayoutLinux::Layout(View* host) {
  if (ShouldShowTitlebarAndBorder()) {
    LayoutWindowControls();
    LayoutTitlebar();
  }

  LayoutClientView();
}

gfx::Size FrameViewLayoutLinux::GetPreferredSize(const View* host) const {
  return GetPreferredSize(host, {});
}

gfx::Size FrameViewLayoutLinux::GetPreferredSize(
    const View* host,
    const SizeBounds& available_size) const {
  Widget* widget = view_->GetWidget();
  return widget->non_client_view()
      ->GetWindowBoundsForClientBounds(
          gfx::Rect(widget->client_view()->GetPreferredSize(available_size)))
      .size();
}

gfx::Size FrameViewLayoutLinux::GetMinimumSize(const View* host) const {
  Widget* widget = view_->GetWidget();
  return widget->non_client_view()
      ->GetWindowBoundsForClientBounds(
          gfx::Rect(widget->client_view()->GetMinimumSize()))
      .size();
}

gfx::Insets FrameViewLayoutLinux::GetFrameBorderInsets() const {
  Widget* widget = view_->GetWidget();
  if (widget->IsMaximized() || widget->IsFullscreen()) {
    return gfx::Insets();
  }
  return GetRestoredFrameBorderInsets();
}

gfx::Insets FrameViewLayoutLinux::GetRestoredFrameBorderInsets() const {
  auto shadow_values = GetShadowValues(/*active=*/true);
  return GetRestoredFrameBorderInsetsLinux(
      supports_client_frame_shadow_, gfx::Insets(kDefaultFrameBorderThickness),
      shadow_values, gfx::Insets(kResizeBorder));
}

gfx::Rect FrameViewLayoutLinux::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect FrameViewLayoutLinux::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Insets insets = GetFrameBorderInsets();
  int top_offset = GetClientTopOffset();
  return gfx::Rect(std::max(0, client_bounds.x() - insets.left()),
                   std::max(0, client_bounds.y() - top_offset),
                   client_bounds.width() + insets.left() + insets.right(),
                   client_bounds.height() + top_offset + insets.bottom());
}

gfx::Insets FrameViewLayoutLinux::GetInputInsets() const {
  Widget* widget = view_->GetWidget();
  if (widget->IsMaximized() || widget->IsFullscreen()) {
    return gfx::Insets();
  }
  // When showing shadows, the shadow border extends well beyond the resize
  // area.  Return the resize band width so the host can exclude the
  // shadow-only area from input.  Without shadows, the entire window
  // receives input.
  return gfx::Insets(supports_client_frame_shadow_ ? kResizeBorder : 0);
}

int FrameViewLayoutLinux::GetTranslucentTopAreaHeight() const {
  return 0;
}

int FrameViewLayoutLinux::GetTopAreaHeight() const {
  if (!ShouldShowTitlebarAndBorder()) {
    return 0;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  int title_font_height = GetTitleFontList().GetHeight();
  int button_height = GetDefaultButtonSize().height();
  int content_height = std::max(
      title_font_height + kDefaultTitlebarVerticalPadding, button_height);

  return insets.top() + content_height;
}

int FrameViewLayoutLinux::GetClientTopOffset() const {
  return GetTopAreaHeight();
}

int FrameViewLayoutLinux::CaptionButtonY() const {
  return GetFrameBorderInsets().top() + GetTopAreaBorderInsets().top();
}

gfx::Insets FrameViewLayoutLinux::GetTopAreaBorderInsets() const {
  return gfx::Insets();
}

FrameViewLayoutLinux::ButtonLayoutParams
FrameViewLayoutLinux::GetButtonLayoutParams(FrameButton button_id,
                                            Button* button) const {
  ButtonLayoutParams params;
  params.size = GetDefaultButtonSize();
  int caption_y = CaptionButtonY();
  int content_height = GetTopAreaHeight() - caption_y;
  params.y = caption_y + (content_height - params.size.height()) / 2;
  return params;
}

gfx::Insets FrameViewLayoutLinux::GetTopAreaSpacing() const {
  return gfx::Insets();
}

gfx::Size FrameViewLayoutLinux::GetDefaultButtonSize() const {
  return GetCaptionButtonLayoutSize(
      CaptionButtonLayoutSize::kNonBrowserCaption);
}

bool FrameViewLayoutLinux::ShouldShowTitlebarAndBorder() const {
  Widget* widget = view_->GetWidget();
  return !widget->IsFullscreen() &&
         !ViewsDelegate::GetInstance()->WindowManagerProvidesTitleBar(
             widget->IsMaximized());
}

void FrameViewLayoutLinux::HideButton(FrameButton button_id) {
  switch (button_id) {
    case FrameButton::kMinimize:
      if (view_->minimize_button()) {
        SetViewVisibility(view_->minimize_button(), false);
      }
      break;
    case FrameButton::kMaximize:
      if (view_->maximize_button()) {
        SetViewVisibility(view_->maximize_button(), false);
      }
      if (view_->restore_button()) {
        SetViewVisibility(view_->restore_button(), false);
      }
      break;
    case FrameButton::kClose:
      if (view_->close_button()) {
        SetViewVisibility(view_->close_button(), false);
      }
      break;
  }
}

bool FrameViewLayoutLinux::ConfigureButton(FrameButton button_id,
                                           bool is_leading,
                                           bool is_first,
                                           int& next_button_x) {
  Widget* widget = view_->GetWidget();

  switch (button_id) {
    case FrameButton::kMinimize: {
      if (!widget->widget_delegate()->CanMinimize()) {
        HideButton(button_id);
        return false;
      }
      break;
    }
    case FrameButton::kMaximize: {
      if (!widget->widget_delegate()->CanMaximize()) {
        HideButton(button_id);
        return false;
      }
      // Hide the inactive maximize/restore button.
      bool is_restored = !widget->IsMaximized() && !widget->IsMinimized();
      Button* inactive_button =
          is_restored ? view_->restore_button() : view_->maximize_button();
      if (inactive_button) {
        SetViewVisibility(inactive_button, false);
      }
      break;
    }
    case FrameButton::kClose:
      break;
  }

  Button* button = view_->GetFrameButton(button_id);
  if (!button) {
    return false;
  }

  auto params = GetButtonLayoutParams(button_id, button);

  if (is_leading) {
    if (!is_first) {
      next_button_x += params.inter_spacing;
    }
    next_button_x += params.margin.left();
    SetButtonVisible(button,
                     gfx::Rect(next_button_x, params.y, params.size.width(),
                               params.size.height()));
    next_button_x += params.size.width() + params.margin.right();
    minimum_titlebar_x_ = std::min(view_->width(), next_button_x);
  } else {
    if (!is_first) {
      next_button_x -= params.inter_spacing;
    }
    next_button_x -= params.margin.right() + params.size.width();
    SetButtonVisible(button,
                     gfx::Rect(next_button_x, params.y, params.size.width(),
                               params.size.height()));
    next_button_x -= params.margin.left();
    maximum_titlebar_x_ = std::max(minimum_titlebar_x_, next_button_x);
  }
  return true;
}

void FrameViewLayoutLinux::LayoutWindowControls() {
  minimum_titlebar_x_ = 0;
  maximum_titlebar_x_ = view_->width();

  if (view_->bounds().IsEmpty()) {
    return;
  }

  // Start by assuming all buttons should be hidden. Buttons that are
  // successfully configured will be made visible; any remaining get hidden.
  std::vector<FrameButton> buttons_not_shown = {
      FrameButton::kMaximize, FrameButton::kMinimize, FrameButton::kClose};

  WindowButtonOrderProvider* button_order =
      WindowButtonOrderProvider::GetInstance();
  const std::vector<FrameButton>& leading_buttons =
      button_order->leading_buttons();
  const std::vector<FrameButton>& trailing_buttons =
      button_order->trailing_buttons();

  gfx::Insets insets = GetFrameBorderInsets();
  gfx::Insets top_spacing = GetTopAreaSpacing();

  // Layout leading buttons.
  int next_button_x = insets.left() + top_spacing.left();
  bool is_first = true;
  for (auto frame_button : leading_buttons) {
    if (ConfigureButton(frame_button, /*is_leading=*/true, is_first,
                        next_button_x)) {
      is_first = false;
    }
    std::erase(buttons_not_shown, frame_button);
  }

  // Layout trailing buttons right-to-left.
  next_button_x = view_->width() - insets.right() - top_spacing.right();
  is_first = true;
  for (auto frame_button : base::Reversed(trailing_buttons)) {
    if (ConfigureButton(frame_button, /*is_leading=*/false, is_first,
                        next_button_x)) {
      is_first = false;
    }
    std::erase(buttons_not_shown, frame_button);
  }

  // Hide any buttons that weren't placed.
  for (auto button_id : buttons_not_shown) {
    HideButton(button_id);
  }
}

void FrameViewLayoutLinux::LayoutTitlebar() {
  CHECK_GE(maximum_titlebar_x_, 0);

  if (!view_->HasWindowTitle()) {
    return;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  int title_height = GetTitleFontList().GetHeight();

  int min_x = minimum_titlebar_x_ + kTitleCaptionSpacing;
  int max_x = maximum_titlebar_x_ - kTitleCaptionSpacing;
  int available_width = std::max(0, max_x - min_x);

  if (available_width <= 0) {
    view_->set_title_bounds(gfx::Rect());
    return;
  }

  gfx::Insets titlebar_border = GetTopAreaBorderInsets();
  int titlebar_content_top = insets.top() + titlebar_border.top();
  int titlebar_content_height =
      GetTopAreaHeight() - insets.top() - titlebar_border.height();
  int title_y =
      titlebar_content_top + (titlebar_content_height - title_height) / 2;

  // Center the title across the full window width, constrained to the
  // available space between the caption buttons.
  int window_width = view_->width();
  int symmetric_min = std::max(min_x, window_width - max_x);
  int label_x = symmetric_min;
  int label_width = std::max(0, window_width - 2 * symmetric_min);

  view_->set_title_bounds(
      gfx::Rect(label_x, title_y, label_width, title_height));
}

void FrameViewLayoutLinux::LayoutClientView() {
  if (!ShouldShowTitlebarAndBorder()) {
    client_view_bounds_ = gfx::Rect(view_->size());
    return;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  int top_offset = GetClientTopOffset();
  client_view_bounds_.SetRect(
      insets.left(), top_offset, std::max(0, view_->width() - insets.width()),
      std::max(0, view_->height() - top_offset - insets.bottom()));
}

}  // namespace views
