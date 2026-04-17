// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_LINUX_H_
#define UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_LINUX_H_

#include <memory>
#include <optional>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/views/views_export.h"
#include "ui/views/window/frame_view_linux.h"
#include "ui/views/window/frame_view_utils_linux.h"
#include "ui/views/window/native_frame_view_layout_linux.h"

namespace views {

class Widget;

// Extends FrameViewLinux to use the native toolkit's WindowFrameProvider for
// frame decoration rendering and NavButtonProvider for window control button
// images.
class VIEWS_EXPORT NativeFrameViewLinux : public FrameViewLinux {
  METADATA_HEADER(NativeFrameViewLinux, FrameViewLinux)

 public:
  // If `layout` is null, a default NativeFrameViewLayoutLinux is created using
  // `nav_button_provider` and `frame_provider_getter`.
  NativeFrameViewLinux(
      Widget* widget,
      std::unique_ptr<ui::NavButtonProvider> nav_button_provider,
      NativeFrameViewLayoutLinux::FrameProviderGetter frame_provider_getter,
      NativeFrameViewLayoutLinux* layout = nullptr);

  NativeFrameViewLinux(const NativeFrameViewLinux&) = delete;
  NativeFrameViewLinux& operator=(const NativeFrameViewLinux&) = delete;

  ~NativeFrameViewLinux() override;

  // View:
  void Layout(PassKey) override;

  // FrameViewLinux:
  void CreateCaptionButtons() override;
  void PaintRestoredFrameBorder(gfx::Canvas* canvas) override;
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas) override;

 protected:
  void OnThemeOrButtonOrderChanged() override;
  void UpdateButtonColors() override;

 private:
  friend class NativeFrameViewLinuxTest;

  NativeFrameViewLayoutLinux* native_layout() const;

  // Redraws the image resources associated with the minimize, maximize,
  // restore, and close buttons.
  void MaybeUpdateCachedFrameButtonImages();

  std::unique_ptr<ui::NavButtonProvider> nav_button_provider_;

  // Cache for the last parameters with which the caption buttons were drawn.
  std::optional<DrawFrameButtonParams> button_cache_;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_LINUX_H_
