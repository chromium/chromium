// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view_linux.h"

#include <algorithm>
#include <utility>

#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_view_utils_linux.h"
#include "ui/views/window/native_frame_view_layout_linux.h"

namespace views {

NativeFrameViewLinux::NativeFrameViewLinux(
    Widget* widget,
    std::unique_ptr<ui::NavButtonProvider> nav_button_provider,
    NativeFrameViewLayoutLinux::FrameProviderGetter frame_provider_getter,
    NativeFrameViewLayoutLinux* layout)
    : FrameViewLinux(widget,
                     layout ? layout
                            : new NativeFrameViewLayoutLinux(
                                  nav_button_provider.get(),
                                  std::move(frame_provider_getter))),
      nav_button_provider_(std::move(nav_button_provider)) {}

NativeFrameViewLinux::~NativeFrameViewLinux() {
  // The layout manager is owned by View and may outlive this destructor. Clear
  // its raw_ptr references to members we own to avoid dangling pointers.
  native_layout()->set_nav_button_provider(nullptr);
}

void NativeFrameViewLinux::Layout(PassKey) {
  MaybeUpdateCachedFrameButtonImages();
  LayoutSuperclass<FrameViewLinux>(this);
}

void NativeFrameViewLinux::CreateCaptionButtons() {
  auto make_button = [this](Button::PressedCallback callback,
                            int accessibility_string_id) -> ImageButton* {
    auto button = std::make_unique<ImageButton>(std::move(callback));
    button->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    button->SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
    button->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(accessibility_string_id));
    return AddChildView(std::move(button));
  };

  close_button_ = make_button(
      base::BindRepeating(&Widget::CloseWithReason,
                          base::Unretained(frame_widget()),
                          Widget::ClosedReason::kCloseButtonClicked),
      IDS_APP_ACCNAME_CLOSE);
  minimize_button_ = make_button(
      base::BindRepeating(&Widget::Minimize, base::Unretained(frame_widget())),
      IDS_APP_ACCNAME_MINIMIZE);
  maximize_button_ = make_button(
      base::BindRepeating(&Widget::Maximize, base::Unretained(frame_widget())),
      IDS_APP_ACCNAME_MAXIMIZE);
  restore_button_ = make_button(
      base::BindRepeating(&Widget::Restore, base::Unretained(frame_widget())),
      IDS_APP_ACCNAME_RESTORE);
}

NativeFrameViewLayoutLinux* NativeFrameViewLinux::native_layout() const {
  return static_cast<NativeFrameViewLayoutLinux*>(layout());
}

void NativeFrameViewLinux::PaintRestoredFrameBorder(gfx::Canvas* canvas) {
  auto* frame_provider = native_layout()->GetFrameProvider();
  if (frame_provider) {
    frame_provider->PaintWindowFrame(
        canvas, GetLocalBounds(), layout()->GetTopAreaHeight(),
        ShouldPaintAsActive(), layout()->GetInputInsets());
  }
}

void NativeFrameViewLinux::PaintMaximizedFrameBorder(gfx::Canvas* canvas) {
  ui::NativeTheme::FrameTopAreaExtraParams frame_top_area;
  frame_top_area.use_custom_frame = true;
  frame_top_area.has_frame_border = true;
  frame_top_area.is_active = ShouldPaintAsActive();
  ui::NativeTheme::ExtraParams params(frame_top_area);
  GetNativeTheme()->Paint(
      canvas->sk_canvas(), GetColorProvider(), ui::NativeTheme::kFrameTopArea,
      ui::NativeTheme::kNormal,
      gfx::Rect(0, 0, width(), layout()->GetTopAreaHeight()), params);
}

void NativeFrameViewLinux::OnThemeOrButtonOrderChanged() {
  button_cache_ = std::nullopt;
  FrameViewLinux::OnThemeOrButtonOrderChanged();
}

void NativeFrameViewLinux::UpdateButtonColors() {
  // No-op: Native button images are provided by NavButtonProvider.
}

void NativeFrameViewLinux::MaybeUpdateCachedFrameButtonImages() {
  int top_border_height = layout()->GetTopAreaHeight();
  gfx::Insets border_insets = layout()->GetFrameBorderInsets();
  gfx::Insets titlebar_border = native_layout()->GetTopAreaBorderInsets();
  DrawFrameButtonParams params{
      top_border_height - border_insets.top() - titlebar_border.height(),
      frame_widget()->IsMaximized(), ShouldPaintAsActive()};
  views::MaybeUpdateCachedFrameButtonImages(
      nav_button_provider_.get(), params, button_cache_,
      [this](ui::NavButtonProvider::FrameButtonDisplayType type)
          -> views::Button* { return GetButtonFromType(type); });
}

BEGIN_METADATA(NativeFrameViewLinux)
END_METADATA

}  // namespace views
