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

// A client interface that `NativeFrameViewMac` can use to augment or override
// its behavior. This allows higher-level modules (Ex: //chrome) to customize
// the frame view without creating a dependency from //ui.
class VIEWS_EXPORT NativeFrameViewMacClient {
 public:
  virtual ~NativeFrameViewMacClient() = default;

  // Returns a hit-test value for the given point, or nullopt to allow default
  // processing. See FrameView::NonClientHitTest for more details.
  virtual std::optional<int> NonClientHitTest(const gfx::Point& point) = 0;
};

class VIEWS_EXPORT NativeFrameViewMac : public NativeFrameView {
  METADATA_HEADER(NativeFrameViewMac, NativeFrameView)

 public:
  NativeFrameViewMac(Widget* widget, NativeFrameViewMacClient* client);
  NativeFrameViewMac(const NativeFrameViewMac&) = delete;
  NativeFrameViewMac& operator=(const NativeFrameViewMac&) = delete;
  ~NativeFrameViewMac() override;

  // FrameView
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;

 private:
  // A client that can customize behavior.
  raw_ptr<NativeFrameViewMacClient, DanglingUntriaged> client_;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_MAC_H_
