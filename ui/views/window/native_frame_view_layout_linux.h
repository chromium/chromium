// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_LAYOUT_LINUX_H_
#define UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_LAYOUT_LINUX_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/views/views_export.h"
#include "ui/views/window/frame_view_layout_linux.h"

namespace ui {
class WindowFrameProvider;
}  // namespace ui

namespace views {

// Layout manager for NativeFrameViewLinux that uses the native toolkit's
// NavButtonProvider for button sizing and margins, and WindowFrameProvider
// for frame geometry. Overrides the base class shadow-based defaults with
// provider-based metrics.
class VIEWS_EXPORT NativeFrameViewLayoutLinux : public FrameViewLayoutLinux {
 public:
  using FrameProviderGetter =
      base::RepeatingCallback<ui::WindowFrameProvider*(bool tiled,
                                                       bool maximized)>;

  NativeFrameViewLayoutLinux(ui::NavButtonProvider* nav_button_provider,
                             FrameProviderGetter frame_provider_getter);

  NativeFrameViewLayoutLinux(const NativeFrameViewLayoutLinux&) = delete;
  NativeFrameViewLayoutLinux& operator=(const NativeFrameViewLayoutLinux&) =
      delete;

  ~NativeFrameViewLayoutLinux() override;

  // Pass nullptr before the provider is destroyed.
  void set_nav_button_provider(ui::NavButtonProvider* provider) {
    nav_button_provider_ = provider;
  }

  // Returns the WindowFrameProvider for the current window state.
  ui::WindowFrameProvider* GetFrameProvider() const;
  gfx::FontList GetTitleFontList() const override;
  gfx::Insets GetRestoredFrameBorderInsets() const override;
  gfx::Insets GetInputInsets() const override;
  int GetTranslucentTopAreaHeight() const override;
  int GetTopAreaHeight() const override;
  gfx::Insets GetTopAreaBorderInsets() const override;
  gfx::ShadowValues GetShadowValues(bool active) const override;
  gfx::RoundedCornersF GetCornerRadii() const override;
  ButtonLayoutParams GetButtonLayoutParams(FrameButton button_id,
                                           Button* button) const override;
  gfx::Insets GetTopAreaSpacing() const override;

 private:
  raw_ptr<ui::NavButtonProvider> nav_button_provider_;
  FrameProviderGetter frame_provider_getter_;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_LAYOUT_LINUX_H_
