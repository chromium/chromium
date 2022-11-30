// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_overlay_view.h"

#include <lib/ui/scenic/cpp/commands.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"

namespace ui {

namespace {

// |buffer_collection_id| that is passed to ImagePipe::AddBufferCollection() if
// SysmemBufferCollection instance is registered with one.
static const uint32_t kImagePipeBufferCollectionId = 1;
static const std::string kSessionDebugName = "chromium scenic overlay";

}  // namespace

ScenicOverlayView::ScenicOverlayView(
    scenic::SessionPtrAndListenerRequest session_and_listener_request)
    : scenic_session_(std::move(session_and_listener_request)),
      safe_presenter_(&scenic_session_),
      image_material_(&scenic_session_) {
  scenic_session_.SetDebugName(kSessionDebugName);
  scenic_session_.set_error_handler(
      base::LogFidlErrorAndExitProcess(FROM_HERE, "ScenicSession"));
}

ScenicOverlayView::~ScenicOverlayView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Releasing |image_pipe_| implicitly also enforces cleanup.
  image_pipe_->RemoveBufferCollection(kImagePipeBufferCollectionId);
}

void ScenicOverlayView::Initialize(
    fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
        collection_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t image_pipe_id = scenic_session_.AllocResourceId();
  scenic_session_.Enqueue(
      scenic::NewCreateImagePipe2Cmd(image_pipe_id, image_pipe_.NewRequest()));
  image_pipe_.set_error_handler(
      base::LogFidlErrorAndExitProcess(FROM_HERE, "ImagePipe"));

  image_material_.SetTexture(image_pipe_id);
  safe_presenter_.QueuePresent();

  // Since there is one ImagePipe for each BufferCollection, it is ok to use a
  // fixed buffer_collection_id.
  // TODO(emircan): Consider using one ImagePipe per video decoder instead.
  image_pipe_->AddBufferCollection(kImagePipeBufferCollectionId,
                                   std::move(collection_token));
}

uint32_t ScenicOverlayView::AddImage(uint32_t buffer_index,
                                     const gfx::Size& size) {
  uint32_t id = next_image_id_++;
  fuchsia::sysmem::ImageFormat_2 image_format = {};
  image_format.coded_width = size.width();
  image_format.coded_height = size.height();
  image_pipe_->AddImage(id, kImagePipeBufferCollectionId, buffer_index,
                        image_format);
  return id;
}

void ScenicOverlayView::RemoveImage(uint32_t image_id) {
  image_pipe_->RemoveImage(image_id);
}

bool ScenicOverlayView::PresentImage(uint32_t image_id,
                                     std::vector<zx::event> acquire_fences,
                                     std::vector<zx::event> release_fences) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  image_pipe_->PresentImage(image_id, zx_clock_get_monotonic(),
                            std::move(acquire_fences),
                            std::move(release_fences), [](auto) {});
  return true;
}

void ScenicOverlayView::SetBlendMode(bool enable_blend) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (enable_blend_ == enable_blend)
    return;

  enable_blend_ = enable_blend;
  // Setting alpha as |255| marks the image as opaque and no content below would
  // be seen. Anything lower than 255 allows blending.
  image_material_.SetColor(255, 255, 255, enable_blend ? 254 : 255);
  safe_presenter_.QueuePresent();
}

void ScenicOverlayView::AttachToScenicSurface(
    fuchsia::ui::views::ViewToken view_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  view_.emplace(&scenic_session_, std::move(view_token), kSessionDebugName);

  scenic::ShapeNode shape(&scenic_session_);
  shape.SetShape(scenic::Rectangle(&scenic_session_, 1.f, 1.f));
  shape.SetMaterial(image_material_);

  view_->AddChild(shape);
  safe_presenter_.QueuePresent();
}

}  // namespace ui
