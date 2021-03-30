// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/eventpair.h>

#include "base/numerics/math_constants.h"
#include "ui/ozone/platform/scenic/scenic_gpu_host.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

namespace ui {

namespace {

// Scenic has z-fighting problems in 3D API, so we add tiny z plane increments
// to elevate content. ViewProperties set by ScenicWindow sets z-plane to
// [-0.5f, 0.5f] range, so 0.01f is small enough to make a difference.
constexpr float kElevationStep = 0.01f;

// Converts OverlayTransform enum to angle in radians.
float OverlayTransformToRadians(gfx::OverlayTransform plane_transform) {
  switch (plane_transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return 0;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return base::kPiFloat * .5f;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return base::kPiFloat;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return base::kPiFloat * 1.5f;
    case gfx::OVERLAY_TRANSFORM_INVALID:
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

ScenicSurface::ScenicSurface(
    ScenicSurfaceFactory* scenic_surface_factory,
    gfx::AcceleratedWidget window,
    scenic::SessionPtrAndListenerRequest sesion_and_listener_request)
    : scenic_session_(std::move(sesion_and_listener_request)),
      main_shape_(&scenic_session_),
      main_material_(&scenic_session_),
      scenic_surface_factory_(scenic_surface_factory),
      window_(window) {
  // Setting alpha to 0 makes this transparent.
  scenic::Material transparent_material(&scenic_session_);
  transparent_material.SetColor(0, 0, 0, 0);
  main_shape_.SetShape(scenic::Rectangle(&scenic_session_, 1.f, 1.f));
  main_shape_.SetMaterial(transparent_material);
  main_shape_.SetEventMask(fuchsia::ui::gfx::kMetricsEventMask);
  scenic_surface_factory->AddSurface(window, this);
  scenic_session_.SetDebugName("Chromium ScenicSurface");
  scenic_session_.set_event_handler(
      fit::bind_member(this, &ScenicSurface::OnScenicEvents));
}

ScenicSurface::~ScenicSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scenic_surface_factory_->RemoveSurface(window_);
}

ScenicSurface::OverlayViewInfo::OverlayViewInfo(scenic::ViewHolder holder,
                                                scenic::EntityNode node)
    : view_holder(std::move(holder)), entity_node(std::move(node)) {}

void ScenicSurface::OnScenicEvents(
    std::vector<fuchsia::ui::scenic::Event> events) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& event : events) {
    DCHECK(event.is_gfx());
    switch (event.gfx().Which()) {
      case fuchsia::ui::gfx::Event::kMetrics: {
        DCHECK(event.gfx().metrics().node_id == main_shape_.id());
        // This is enough to track size because |main_shape_| is 1x1.
        const auto& metrics = event.gfx().metrics().metrics;
        main_shape_size_.set_width(metrics.scale_x);
        main_shape_size_.set_height(metrics.scale_y);
        UpdateViewHolderScene();
        break;
      }
      default:
        break;
    }
  }
}

bool ScenicSurface::SetTextureToNewImagePipe(
    fidl::InterfaceRequest<fuchsia::images::ImagePipe2> image_pipe_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint32_t image_pipe_id = scenic_session_.AllocResourceId();
  scenic_session_.Enqueue(scenic::NewCreateImagePipe2Cmd(
      image_pipe_id, std::move(image_pipe_request)));
  main_material_.SetTexture(image_pipe_id);
  main_shape_.SetMaterial(main_material_);
  scenic_session_.ReleaseResource(image_pipe_id);
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
  return true;
}

void ScenicSurface::SetTextureToImage(const scenic::Image& image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  main_material_.SetTexture(image);
  main_shape_.SetMaterial(main_material_);
}

bool ScenicSurface::PresentOverlayView(
    gfx::SysmemBufferCollectionId id,
    fuchsia::ui::views::ViewHolderToken view_holder_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scenic::ViewHolder view_holder(&scenic_session_, std::move(view_holder_token),
                                 "OverlayViewHolder");
  scenic::EntityNode entity_node(&scenic_session_);
  fuchsia::ui::gfx::ViewProperties view_properties;
  view_properties.bounding_box = {{-0.5f, -0.5f, 0.f}, {0.5f, 0.5f, 0.f}};
  view_properties.focus_change = false;
  view_holder.SetViewProperties(std::move(view_properties));
  view_holder.SetHitTestBehavior(fuchsia::ui::gfx::HitTestBehavior::kSuppress);

  entity_node.AddChild(view_holder);
  parent_->AddChild(entity_node);
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});

  DCHECK(!overlays_.count(id));
  overlays_.emplace(
      std::piecewise_construct, std::forward_as_tuple(id),
      std::forward_as_tuple(std::move(view_holder), std::move(entity_node)));

  return true;
}

bool ScenicSurface::UpdateOverlayViewPosition(
    gfx::SysmemBufferCollectionId id,
    int plane_z_order,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect,
    gfx::OverlayTransform plane_transform,
    std::vector<zx::event> acquire_fences) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(overlays_.count(id));
  auto& overlay_view_info = overlays_.at(id);

  if (overlay_view_info.plane_z_order == plane_z_order &&
      overlay_view_info.display_bounds == display_bounds &&
      overlay_view_info.crop_rect == crop_rect &&
      overlay_view_info.plane_transform == plane_transform) {
    return false;
  }

  overlay_view_info.plane_z_order = plane_z_order;
  overlay_view_info.display_bounds = display_bounds;
  overlay_view_info.crop_rect = crop_rect;
  overlay_view_info.plane_transform = plane_transform;

  for (auto& fence : acquire_fences)
    scenic_session_.EnqueueAcquireFence(std::move(fence));

  // TODO(crbug.com/1143514): Only queue commands for the affected overlays
  // instead of the whole scene.
  UpdateViewHolderScene();

  return true;
}

bool ScenicSurface::RemoveOverlayView(gfx::SysmemBufferCollectionId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = overlays_.find(id);
  DCHECK(it != overlays_.end());
  parent_->DetachChild(it->second.entity_node);
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
  overlays_.erase(it);
  return true;
}

mojo::PlatformHandle ScenicSurface::CreateView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Scenic will associate the View and ViewHolder regardless of which it
  // learns about first, so we don't need to synchronize View creation with
  // attachment into the scene graph by the caller.
  auto tokens = scenic::ViewTokenPair::New();
  parent_ = std::make_unique<scenic::View>(
      &scenic_session_, std::move(tokens.view_token), "chromium surface");
  parent_->AddChild(main_shape_);

  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
  return mojo::PlatformHandle(std::move(tokens.view_holder_token.value));
}

void ScenicSurface::UpdateViewHolderScene() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (overlays_.empty())
    return;

  // |plane_z_order| for main surface is 0.
  int min_z_order = 0;
  for (const auto& overlay : overlays_) {
    min_z_order = std::min(overlay.second.plane_z_order, min_z_order);
  }
  for (auto& overlay : overlays_) {
    auto& info = overlay.second;

    // Apply view bound clipping around the ImagePipe that has size 1x1 and
    // centered at (0, 0).
    fuchsia::ui::gfx::ViewProperties view_properties;
    const float left_bound = -0.5f + info.crop_rect.x();
    const float top_bound = -0.5f + info.crop_rect.y();
    view_properties.bounding_box = {{left_bound, top_bound, 0.f},
                                    {left_bound + info.crop_rect.width(),
                                     top_bound + info.crop_rect.height(), 0.f}};
    view_properties.focus_change = false;
    info.view_holder.SetViewProperties(std::move(view_properties));

    // We receive |display_bounds| in screen coordinates. Convert them to fit
    // 1x1 View, which is later scaled up by the browser process.
    float scaled_width = info.display_bounds.width() /
                         (info.crop_rect.width() * main_shape_size_.width());
    float scaled_height = info.display_bounds.height() /
                          (info.crop_rect.height() * main_shape_size_.height());
    const float scaled_x = info.display_bounds.x() / main_shape_size_.width();
    const float scaled_y = info.display_bounds.y() / main_shape_size_.height();

    // Position ImagePipe based on the display bounds given.
    info.entity_node.SetTranslation(
        -0.5f + scaled_x + scaled_width / 2,
        -0.5f + scaled_y + scaled_height / 2,
        (min_z_order - info.plane_z_order) * kElevationStep);

    // Apply rotation if given. Scenic expects rotation passed as Quaternion.
    const float angle = OverlayTransformToRadians(info.plane_transform);
    info.entity_node.SetRotation(
        {0.f, 0.f, sinf(angle * .5f), cosf(angle * .5f)});

    // Scenic applies scaling before rotation.
    if (info.plane_transform == gfx::OVERLAY_TRANSFORM_ROTATE_90 ||
        info.plane_transform == gfx::OVERLAY_TRANSFORM_ROTATE_270) {
      std::swap(scaled_width, scaled_height);
    }

    // Scenic expects flip as negative scaling.
    if (info.plane_transform == gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL) {
      scaled_width = -scaled_width;
    } else if (info.plane_transform == gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL) {
      scaled_height = -scaled_height;
    }

    // Scale ImagePipe based on the display bounds and clip rect given.
    info.entity_node.SetScale(scaled_width, scaled_height, 1.f);
  }

  main_material_.SetColor(255, 255, 255, 0 > min_z_order ? 254 : 255);
  main_shape_.SetTranslation(0.f, 0.f, min_z_order * kElevationStep);

  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
}

}  // namespace ui
