// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>

#include "base/threading/thread_checker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/scenic/safe_presenter.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"

namespace ui {

// Holder for scenic::Session and scenic::View that owns an Image Pipe.
//
// This class allows the callers to access an ImagePipe and a scenic::View that
// displays only that ImagePipe. This is used inside SysmemBufferCollection
// instances to display overlays.
class ScenicOverlayView {
 public:
  explicit ScenicOverlayView(
      scenic::SessionPtrAndListenerRequest session_and_listener_request);
  ~ScenicOverlayView();
  ScenicOverlayView(const ScenicOverlayView&) = delete;
  ScenicOverlayView& operator=(const ScenicOverlayView&) = delete;

  // Calls ImagePipe2::AddBufferCollection() using |collection_token|. All
  // |image_pipe| interactions in this class is then associated with this
  // BufferCollection.
  void Initialize(fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
                      collection_token);

  // Adds an image to the ImagePipe using the specified `buffer_index` and
  // `size`. Returns a non-zero `image_id` that may be used in `PresentImage()`
  // and `RemoveImage()`.
  uint32_t AddImage(uint32_t buffer_index, const gfx::Size& size);

  // Removes image with the `image_id_` from the ImagePipe.
  void RemoveImage(uint32_t image_id);

  // Calls `ImagePipe2::PresentImage()` on `image_pipe_` for the `image_id`
  // corresponding to |buffer_index| from the initialized BufferCollection.
  bool PresentImage(uint32_t image_id,
                    std::vector<zx::event> acquire_fences,
                    std::vector<zx::event> release_fences);

  // If |enable_blend| is true, sets |image_pipe_| as non-opaque.
  void SetBlendMode(bool enable_blend);

  // Attaches the view using the specified token.
  void AttachToScenicSurface(fuchsia::ui::views::ViewToken view_token);

 private:
  scenic::Session scenic_session_;
  // Used for safely queueing Present() operations on |scenic_session_|.
  SafePresenter safe_presenter_;
  absl::optional<scenic::View> view_;
  fuchsia::images::ImagePipe2Ptr image_pipe_;
  scenic::Material image_material_;

  uint32_t next_image_id_ = 1;

  bool enable_blend_ = false;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_
