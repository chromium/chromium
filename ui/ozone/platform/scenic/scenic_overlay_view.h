// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>

#include "base/threading/thread_checker.h"

namespace ui {

// Holder for scenic::Session and scenic::View that owns an Image Pipe.
//
// This class allows the callers to access an ImagePipe and a scenic::View that
// displays only that ImagePipe. This is used inside SysmemBufferCollection
// instances to display overlays.
class ScenicOverlayView {
 public:
  ScenicOverlayView(
      scenic::SessionPtrAndListenerRequest session_and_listener_request);
  ~ScenicOverlayView();
  ScenicOverlayView(const ScenicOverlayView&) = delete;
  ScenicOverlayView& operator=(const ScenicOverlayView&) = delete;

  void Initialize(fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
                      collection_token);

 private:
  scenic::Session scenic_session_;
  fuchsia::ui::views::ViewHolderToken view_holder_token_;
  scenic::View view_;
  fuchsia::images::ImagePipe2Ptr image_pipe_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_
