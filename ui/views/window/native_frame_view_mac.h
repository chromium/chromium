// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_MAC_H_
#define UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_MAC_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/native_frame_view.h"

namespace views {

class Widget;

class VIEWS_EXPORT NativeFrameViewMac : public NativeFrameView {
  METADATA_HEADER(NativeFrameViewMac, NativeFrameView)

 public:
  explicit NativeFrameViewMac(Widget* widget);
  NativeFrameViewMac(const NativeFrameViewMac&) = delete;
  NativeFrameViewMac& operator=(const NativeFrameViewMac&) = delete;
  ~NativeFrameViewMac() override;

  // FrameView
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_MAC_H_
