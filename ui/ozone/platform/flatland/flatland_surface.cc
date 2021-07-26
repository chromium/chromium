// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface.h"

#include <lib/zx/eventpair.h>

#include "ui/ozone/platform/flatland/flatland_gpu_host.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"

namespace ui {

FlatlandSurface::FlatlandSurface(
    FlatlandSurfaceFactory* flatland_surface_factory,
    gfx::AcceleratedWidget window)
    : flatland_surface_factory_(flatland_surface_factory), window_(window) {
  flatland_.flatland()->SetDebugName("Chromium FlatlandSurface");

  // Create a transform and make it the root.
  root_transform_id_ = {++next_id_};
  flatland_.flatland()->CreateTransform(root_transform_id_);
  flatland_.flatland()->SetRootTransform(root_transform_id_);
  flatland_.QueuePresent();
}

FlatlandSurface::~FlatlandSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  flatland_surface_factory_->RemoveSurface(window_);
}

bool FlatlandSurface::SetTextureToNewImagePipe(
    fidl::InterfaceRequest<fuchsia::images::ImagePipe2> image_pipe_request) {
  return false;
}

mojo::PlatformHandle FlatlandSurface::CreateView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  fuchsia::ui::composition::ContentLinkToken parent_token;
  fuchsia::ui::composition::GraphLinkToken child_token;
  auto status = zx::channel::create(0, &parent_token.value, &child_token.value);
  CHECK_EQ(status, ZX_OK);

  flatland_.flatland()->LinkToParent(std::move(child_token),
                                     graph_link_to_parent_.NewRequest());
  graph_link_to_parent_->GetLayout(
      fit::bind_member(this, &FlatlandSurface::OnGetLayout));
  return mojo::PlatformHandle(std::move(parent_token.value));
}

void FlatlandSurface::OnGetLayout(fuchsia::ui::composition::LayoutInfo info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  layout_info_ = std::move(info);

  graph_link_to_parent_->GetLayout(
      fit::bind_member(this, &FlatlandSurface::OnGetLayout));
}

}  // namespace ui
