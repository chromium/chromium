// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_overlay_view.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

namespace ui {

namespace {

// |buffer_collection_id| that is passed to ImagePipe::AddBufferCollection() if
// SysmemBufferCollection instance is registered with one.
static const uint32_t kImagePipeBufferCollectionId = 1;
static const std::string kSessionDebugName = "chromium scenic overlay";

fuchsia::ui::views::ViewToken CreateViewToken(
    fuchsia::ui::views::ViewHolderToken* holder_token) {
  auto token_pair = scenic::ViewTokenPair::New();
  *holder_token = std::move(token_pair.view_holder_token);
  return std::move(token_pair.view_token);
}

}  // namespace

ScenicOverlayView::ScenicOverlayView(
    scenic::SessionPtrAndListenerRequest session_and_listener_request,
    ScenicSurfaceFactory* scenic_surface_factory)
    : scenic_session_(std::move(session_and_listener_request)),
      scenic_surface_factory_(scenic_surface_factory),
      view_(&scenic_session_,
            CreateViewToken(&view_holder_token_),
            kSessionDebugName) {
  scenic_session_.SetDebugName(kSessionDebugName);
  scenic_session_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "Lost connection to scenic session.";
  });
}

ScenicOverlayView::~ScenicOverlayView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScenicSurface* surface = scenic_surface_factory_->GetSurface(widget_);
  if (surface) {
    surface->AssertBelongsToCurrentThread();
    surface->RemoveOverlayView(buffer_collection_id_);
  }

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
  image_pipe_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "ImagePipe disconnected";
  });

  image_material_ = std::make_unique<scenic::Material>(&scenic_session_);
  image_material_->SetTexture(image_pipe_id);

  scenic::ShapeNode shape(&scenic_session_);
  shape.SetShape(scenic::Rectangle(&scenic_session_, 1.f, 1.f));
  shape.SetMaterial(*image_material_);

  view_.AddChild(shape);
  scenic_session_.ReleaseResource(image_pipe_id);
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});

  // Since there is one ImagePipe for each BufferCollection, it is ok to use a
  // fixed buffer_collection_id.
  // TODO(emircan): Consider using one ImagePipe per video decoder instead.
  image_pipe_->AddBufferCollection(kImagePipeBufferCollectionId,
                                   std::move(collection_token));
}

bool ScenicOverlayView::AddImages(uint32_t buffer_count,
                                  const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  fuchsia::sysmem::ImageFormat_2 image_format = {};
  image_format.coded_width = size.width();
  image_format.coded_height = size.height();
  for (uint32_t i = 0; i < buffer_count; ++i) {
    // Image id cannot be 0, so add 1 to all buffer indices.
    image_pipe_->AddImage(i + 1, kImagePipeBufferCollectionId, i, image_format);
  }
  return true;
}

bool ScenicOverlayView::PresentImage(uint32_t buffer_index,
                                     std::vector<zx::event> acquire_fences,
                                     std::vector<zx::event> release_fences) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  image_pipe_->PresentImage(buffer_index + 1, zx_clock_get_monotonic(),
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
  image_material_->SetColor(255, 255, 255, enable_blend ? 254 : 255);
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
}

bool ScenicOverlayView::CanAttachToAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return view_holder_token_.value.is_valid() || (widget_ == widget);
}

bool ScenicOverlayView::AttachToScenicSurface(
    gfx::AcceleratedWidget widget,
    gfx::SysmemBufferCollectionId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (widget_ != gfx::kNullAcceleratedWidget && widget_ == widget)
    return true;

  if (!view_holder_token_.value.is_valid()) {
    DLOG(ERROR) << "ViewHolder is already attached.";
    return false;
  }

  buffer_collection_id_ = id;
  widget_ = widget;

  ScenicSurface* surface = scenic_surface_factory_->GetSurface(widget_);
  DCHECK(surface);
  return surface->PresentOverlayView(buffer_collection_id_,
                                     std::move(view_holder_token_));
}

}  // namespace ui
