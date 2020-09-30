// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_overlay_view.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/fuchsia/fuchsia_logging.h"

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
    scenic::SessionPtrAndListenerRequest session_and_listener_request)
    : scenic_session_(std::move(session_and_listener_request)),
      view_(&scenic_session_,
            CreateViewToken(&view_holder_token_),
            kSessionDebugName) {
  scenic_session_.SetDebugName(kSessionDebugName);
  scenic_session_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "Lost connection to scenic session.";
  });
}

ScenicOverlayView::~ScenicOverlayView() = default;

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

  scenic::Material image_material(&scenic_session_);
  image_material.SetTexture(image_pipe_id);

  scenic::ShapeNode shape(&scenic_session_);
  shape.SetShape(scenic::Rectangle(&scenic_session_, 1.f, 1.f));
  shape.SetMaterial(image_material);

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
  LOG(ERROR) << __func__;
}

}  // namespace ui
