// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/eventpair.h>

#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/platform/scenic/scenic_gpu_host.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

namespace ui {

ScenicSurface::ScenicSurface(
    ScenicSurfaceFactory* scenic_surface_factory,
    gfx::AcceleratedWidget window,
    scenic::SessionPtrAndListenerRequest sesion_and_listener_request)
    : scenic_session_(std::move(sesion_and_listener_request)),
      shape_(&scenic_session_),
      material_(&scenic_session_),
      scenic_surface_factory_(scenic_surface_factory),
      window_(window) {
  shape_.SetShape(scenic::Rectangle(&scenic_session_, 1.f, 1.f));
  shape_.SetMaterial(material_);
  scenic_surface_factory->AddSurface(window, this);
  scenic_session_.SetDebugName("Chromium ScenicSurface");
}

ScenicSurface::~ScenicSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scenic_surface_factory_->RemoveSurface(window_);
}

void ScenicSurface::SetTextureToNewImagePipe1(
    fidl::InterfaceRequest<fuchsia::images::ImagePipe> image_pipe_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint32_t image_pipe_id = scenic_session_.AllocResourceId();
  scenic_session_.Enqueue(scenic::NewCreateImagePipeCmd(
      image_pipe_id, std::move(image_pipe_request)));
  material_.SetTexture(image_pipe_id);
  scenic_session_.ReleaseResource(image_pipe_id);
  scenic_session_.Present(
      /*presentation_time=*/0, [](fuchsia::images::PresentationInfo info) {});
}

void ScenicSurface::SetTextureToNewImagePipe(
    fidl::InterfaceRequest<fuchsia::images::ImagePipe2> image_pipe_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint32_t image_pipe_id = scenic_session_.AllocResourceId();
  scenic_session_.Enqueue(scenic::NewCreateImagePipe2Cmd(
      image_pipe_id, std::move(image_pipe_request)));
  material_.SetTexture(image_pipe_id);
  scenic_session_.ReleaseResource(image_pipe_id);
  scenic_session_.Present(
      /*presentation_time=*/0, [](fuchsia::images::PresentationInfo info) {});
}

void ScenicSurface::SetTextureToImage(const scenic::Image& image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  material_.SetTexture(image);
}

mojo::ScopedHandle ScenicSurface::CreateView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Scenic will associate the View and ViewHolder regardless of which it
  // learns about first, so we don't need to synchronize View creation with
  // attachment into the scene graph by the caller.
  auto tokens = scenic::ViewTokenPair::New();
  parent_ = std::make_unique<scenic::View>(
      &scenic_session_, std::move(tokens.view_token), "chromium surface");
  parent_->AddChild(shape_);

  scenic_session_.Present(
      /*presentation_time=*/0, [](fuchsia::images::PresentationInfo info) {});
  return mojo::WrapPlatformHandle(
      mojo::PlatformHandle(std::move(tokens.view_holder_token.value)));
}

}  // namespace ui
