// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <memory>

#include "base/threading/thread_checker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"

namespace ui {

// Holder for scenic::Session and scenic::View that owns an Image Pipe.
//
// This class allows the callers to access an ImagePipe and a scenic::View that
// displays only that ImagePipe. This is used inside SysmemBufferCollection
// instances to display overlays.
class ScenicOverlayView {
 public:
  ScenicOverlayView(
      scenic::SessionPtrAndListenerRequest session_and_listener_request,
      ScenicSurfaceFactory* scenic_surface_factory);
  ~ScenicOverlayView();
  ScenicOverlayView(const ScenicOverlayView&) = delete;
  ScenicOverlayView& operator=(const ScenicOverlayView&) = delete;

  // Calls ImagePipe2::AddBufferCollection() using |collection_token|. All
  // |image_pipe| interactions in this class is then associated with this
  // BufferCollection.
  void Initialize(fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
                      collection_token);

  // Calls ImagePipe2::AddImage() on |image_pipe_| for all of |buffer_count|
  // images from the initialized BufferCollection with the specified |size|.
  bool AddImages(uint32_t buffer_count, const gfx::Size& size);

  // Calls ImagePipe2::PresentImage() on |image_pipe_| for the image
  // corresponding to |buffer_index| from the initialized BufferCollection.
  bool PresentImage(uint32_t buffer_index,
                    std::vector<zx::event> acquire_fences,
                    std::vector<zx::event> release_fences);

  // If |enable_blend| is true, sets |image_pipe_| as non-opaque.
  void SetBlendMode(bool enable_blend);

  // Return true if |view_holder_token_| can be attached to a surface from
  // |widget|.
  bool CanAttachToAcceleratedWidget(gfx::AcceleratedWidget widget);

  // Return true if |view_holder_token_| is attached to the scene graph of
  // surface corresponding to |widget|.
  bool AttachToScenicSurface(gfx::AcceleratedWidget widget,
                             gfx::SysmemBufferCollectionId id);

 private:
  scenic::Session scenic_session_;
  ScenicSurfaceFactory* const scenic_surface_factory_;
  fuchsia::ui::views::ViewHolderToken view_holder_token_;
  scenic::View view_;
  fuchsia::images::ImagePipe2Ptr image_pipe_;
  std::unique_ptr<scenic::Material> image_material_;

  bool enable_blend_ = false;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  gfx::SysmemBufferCollectionId buffer_collection_id_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_OVERLAY_VIEW_H_
