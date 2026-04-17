// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view_layout_linux.h"

#include <algorithm>
#include <utility>

#include "ui/gfx/font_list.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_view_linux.h"

namespace views {

NativeFrameViewLayoutLinux::NativeFrameViewLayoutLinux(
    ui::NavButtonProvider* nav_button_provider,
    FrameProviderGetter frame_provider_getter)
    : nav_button_provider_(nav_button_provider),
      frame_provider_getter_(std::move(frame_provider_getter)) {}

NativeFrameViewLayoutLinux::~NativeFrameViewLayoutLinux() = default;

ui::WindowFrameProvider* NativeFrameViewLayoutLinux::GetFrameProvider() const {
  return frame_provider_getter_.Run(tiled(),
                                    view()->GetWidget()->IsMaximized());
}

gfx::FontList NativeFrameViewLayoutLinux::GetTitleFontList() const {
  return TypographyProvider::Get().GetWindowTitleFontList().DeriveWithWeight(
      gfx::Font::Weight::BOLD);
}

gfx::Insets NativeFrameViewLayoutLinux::GetRestoredFrameBorderInsets() const {
  auto* frame_provider =
      frame_provider_getter_.Run(tiled(), /*maximized=*/false);
  gfx::Insets insets = frame_provider->GetFrameThicknessDip();
  if (supports_client_frame_shadow()) {
    insets.SetToMax(gfx::Insets(kResizeBorder));
  }
  return insets;
}

gfx::Insets NativeFrameViewLayoutLinux::GetInputInsets() const {
  Widget* widget = view()->GetWidget();
  if (widget->IsMaximized() || widget->IsFullscreen()) {
    return gfx::Insets();
  }
  return gfx::Insets(supports_client_frame_shadow() ? kResizeBorder : 0);
}

int NativeFrameViewLayoutLinux::GetTranslucentTopAreaHeight() const {
  auto* provider = GetFrameProvider();
  return (provider && provider->IsTopFrameTranslucent()) ? GetTopAreaHeight()
                                                         : 0;
}

int NativeFrameViewLayoutLinux::GetTopAreaHeight() const {
  if (!ShouldShowTitlebarAndBorder()) {
    return 0;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  int title_font_height = GetTitleFontList().GetHeight();

  auto* frame_provider = GetFrameProvider();
  int titlebar_min_height = frame_provider->GetTopAreaMinHeightDip();
  gfx::Insets titlebar_padding = frame_provider->GetTopAreaPaddingDip();
  gfx::Insets titlebar_border = frame_provider->GetTopAreaBorderDip();

  int button_content_height = nav_button_provider_->GetNavButtonHeight(
      view()->GetWidget()->IsMaximized());
  int content_height =
      std::max({title_font_height, titlebar_min_height, button_content_height});

  return insets.top() + titlebar_border.top() + titlebar_padding.top() +
         content_height + titlebar_padding.bottom() + titlebar_border.bottom();
}

gfx::Insets NativeFrameViewLayoutLinux::GetTopAreaBorderInsets() const {
  return GetFrameProvider()->GetTopAreaBorderDip();
}

gfx::ShadowValues NativeFrameViewLayoutLinux::GetShadowValues(
    bool /*active*/) const {
  // No-op: shadows are drawn by the WindowFrameProvider.
  return gfx::ShadowValues();
}

gfx::RoundedCornersF NativeFrameViewLayoutLinux::GetCornerRadii() const {
  auto* frame_provider = GetFrameProvider();
  float radius = frame_provider ? frame_provider->GetTopCornerRadiusDip() : 0;
  return gfx::RoundedCornersF(radius, radius, 0, 0);
}

FrameViewLayoutLinux::ButtonLayoutParams
NativeFrameViewLayoutLinux::GetButtonLayoutParams(FrameButton button_id,
                                                  Button* button) const {
  auto display_type = view()->GetButtonDisplayType(button_id);

  ButtonLayoutParams params;
  params.margin = nav_button_provider_->GetNavButtonMargin(display_type);
  params.size = button->GetPreferredSize({});
  params.inter_spacing = nav_button_provider_->GetInterNavButtonSpacing();
  params.y = CaptionButtonY() + params.margin.top();
  return params;
}

gfx::Insets NativeFrameViewLayoutLinux::GetTopAreaSpacing() const {
  return nav_button_provider_->GetTopAreaSpacing();
}

}  // namespace views
