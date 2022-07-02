// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/zx/eventpair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/ozone/platform/flatland/flatland_gpu_host.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"
#include "ui/ozone/public/overlay_plane.h"

namespace ui {

namespace {

// Default interval used for vsync callback.
// TODO(fxbug.dev/93998): Remove the usage of this by calculating fps through
// Display API and present callbacks.
constexpr base::TimeDelta kDefaultVsyncInterval = base::Seconds(1) / 60;

std::vector<zx::event> GpuFenceHandlesToZxEvents(
    std::vector<gfx::GpuFenceHandle> handles) {
  std::vector<zx::event> events;
  events.reserve(handles.size());
  for (auto& handle : handles)
    events.push_back(std::move(handle.owned_event));
  return events;
}

zx::event DuplicateZxEvent(const zx::event& event) {
  zx::event result;
  zx_status_t status = event.duplicate(ZX_RIGHT_SAME_RIGHTS, &result);
  ZX_DCHECK(status == ZX_OK, status);
  return result;
}

// Converts OverlayTransform enum to angle in radians.
fuchsia::ui::composition::Orientation OverlayTransformToOrientation(
    gfx::OverlayTransform plane_transform) {
  switch (plane_transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return fuchsia::ui::composition::Orientation::CCW_0_DEGREES;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return fuchsia::ui::composition::Orientation::CCW_90_DEGREES;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return fuchsia::ui::composition::Orientation::CCW_180_DEGREES;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return fuchsia::ui::composition::Orientation::CCW_270_DEGREES;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
    case gfx::OVERLAY_TRANSFORM_INVALID:
      break;
  }
  NOTREACHED();
  return fuchsia::ui::composition::Orientation::CCW_0_DEGREES;
}

fuchsia::math::SizeU GfxSizeToFuchsiaSize(const gfx::Size& size) {
  return fuchsia::math::SizeU{static_cast<uint32_t>(size.width()),
                              static_cast<uint32_t>(size.height())};
}

}  // namespace

FlatlandSurface::FlatlandSurface(
    FlatlandSurfaceFactory* flatland_surface_factory,
    gfx::AcceleratedWidget window)
    : flatland_("Chromium FlatlandSurface"),
      flatland_surface_factory_(flatland_surface_factory),
      window_(window) {
  // Create Flatland Allocator connection.
  flatland_allocator_ = base::ComponentContextForProcess()
                            ->svc()
                            ->Connect<fuchsia::ui::composition::Allocator>();
  flatland_allocator_.set_error_handler(base::LogFidlErrorAndExitProcess(
      FROM_HERE, "fuchsia::ui::composition::Allocator"));

  // Create a transform and make it the root.
  root_transform_id_ = flatland_.NextTransformId();
  flatland_.flatland()->CreateTransform(root_transform_id_);
  flatland_.flatland()->SetRootTransform(root_transform_id_);
  primary_plane_transform_id_ = flatland_.NextTransformId();
  flatland_.flatland()->CreateTransform(primary_plane_transform_id_);

  flatland_surface_factory_->AddSurface(window, this);
}

FlatlandSurface::~FlatlandSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Signal release fences that were submitted in the last PresentImage(). This
  // is necessary because ExternalVkImageBacking destructor will wait for the
  // corresponding semaphores, while they may not be signaled by Flatland.
  for (auto& fence : release_fences_from_last_present_) {
    auto status =
        fence.signal(/*clear_mask=*/0, /*set_mask=*/ZX_EVENT_SIGNALED);
    ZX_DCHECK(status == ZX_OK, status);
  }

  flatland_surface_factory_->RemoveSurface(window_);
}

void FlatlandSurface::Present(
    scoped_refptr<gfx::NativePixmap> primary_plane_pixmap,
    std::vector<ui::OverlayPlane> overlays,
    std::vector<gfx::GpuFenceHandle> acquire_fences,
    std::vector<gfx::GpuFenceHandle> release_fences,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  // Start a new frame by clearing all child transforms.
  ClearScene();

  // Prepare overlay planes.
  for (auto& overlay : overlays) {
    const auto flatland_ids = CreateOrGetFlatlandIds(
        overlay.pixmap.get(), /*is_primary_plane=*/false);
    const auto image_id = flatland_ids.image_id;
    const auto transform_id = flatland_ids.transform_id;
    if (overlay.gpu_fence)
      acquire_fences.push_back(overlay.gpu_fence->GetGpuFenceHandle().Clone());
    child_transforms_[overlay.overlay_plane_data.z_order] = transform_id;

    flatland_.flatland()->SetOrientation(
        transform_id, OverlayTransformToOrientation(
                          overlay.overlay_plane_data.plane_transform));
    const auto rounded_bounds =
        gfx::ToRoundedRect(overlay.overlay_plane_data.display_bounds);
    const fuchsia::math::Vec translation = {rounded_bounds.x(),
                                            rounded_bounds.y()};
    flatland_.flatland()->SetTranslation(transform_id, translation);
    flatland_.flatland()->SetImageDestinationSize(
        image_id, GfxSizeToFuchsiaSize(rounded_bounds.size()));
    const gfx::Size image_size =
        static_cast<FlatlandSysmemNativePixmap*>(overlay.pixmap.get())
            ->sysmem_buffer_collection()
            ->size();
    gfx::RectF crop_rect = overlay.overlay_plane_data.crop_rect;
    crop_rect.Scale(image_size.width(), image_size.height());
    const auto rounded_crop_rect = gfx::ToRoundedRect(crop_rect);
    fuchsia::math::Rect clip_rect = {
        rounded_crop_rect.x(), rounded_crop_rect.y(), rounded_crop_rect.width(),
        rounded_crop_rect.height()};
    flatland_.flatland()->SetClipBoundary(
        transform_id,
        std::make_unique<fuchsia::math::Rect>(std::move(clip_rect)));
    flatland_.flatland()->SetImageBlendingFunction(
        image_id, overlay.overlay_plane_data.enable_blend
                      ? fuchsia::ui::composition::BlendMode::SRC_OVER
                      : fuchsia::ui::composition::BlendMode::SRC);
    flatland_.flatland()->SetImageOpacity(image_id,
                                          overlay.overlay_plane_data.opacity);
  }

  // Prepare primary plane.
  const auto primary_plane_image_id =
      CreateOrGetFlatlandIds(primary_plane_pixmap.get(),
                             /*is_primary_plane=*/true)
          .image_id;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "viz", "FlatlandSurface::Present", TRACE_ID_LOCAL(this),
      "primary_plane_image_id", primary_plane_image_id.value);
  child_transforms_[0] = primary_plane_transform_id_;
  flatland_.flatland()->SetImageDestinationSize(primary_plane_image_id,
                                                layout_info_.logical_size());
  flatland_.flatland()->SetContent(primary_plane_transform_id_,
                                   primary_plane_image_id);
  // TODO(crbug.com/1330950): We should set SRC blend mode when Chrome has a
  // reliable signal for opaque background.
  flatland_.flatland()->SetImageBlendingFunction(
      primary_plane_image_id, fuchsia::ui::composition::BlendMode::SRC_OVER);

  // Add children in z-order.
  for (auto& child : child_transforms_) {
    flatland_.flatland()->AddChild(root_transform_id_, child.second);
  }

  // Add to pending frame to track callbacks.
  pending_frames_.emplace_back(
      primary_plane_image_id, std::move(primary_plane_pixmap),
      std::move(completion_callback), std::move(presentation_callback));

  // Keep track of release fences from last present for destructor.
  release_fences_from_last_present_.clear();
  for (auto& fence : release_fences) {
    release_fences_from_last_present_.push_back(
        DuplicateZxEvent(fence.owned_event));
  }

  // Present to Flatland.
  fuchsia::ui::composition::PresentArgs present_args;
  present_args.set_requested_presentation_time(0);
  present_args.set_acquire_fences(
      GpuFenceHandlesToZxEvents(std::move(acquire_fences)));
  present_args.set_release_fences(
      GpuFenceHandlesToZxEvents(std::move(release_fences)));
  present_args.set_unsquashable(false);
  flatland_.Present(std::move(present_args),
                    base::BindOnce(&FlatlandSurface::OnPresentComplete,
                                   base::Unretained(this)));
}

mojo::PlatformHandle FlatlandSurface::CreateView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  fuchsia::ui::views::ViewportCreationToken parent_token;
  fuchsia::ui::views::ViewCreationToken child_token;
  auto status = zx::channel::create(0, &parent_token.value, &child_token.value);
  DCHECK_EQ(status, ZX_OK);

  flatland_.flatland()->CreateView(std::move(child_token),
                                   parent_viewport_watcher_.NewRequest());
  parent_viewport_watcher_->GetLayout(
      fit::bind_member(this, &FlatlandSurface::OnGetLayout));
  return mojo::PlatformHandle(std::move(parent_token.value));
}

void FlatlandSurface::OnGetLayout(fuchsia::ui::composition::LayoutInfo info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  layout_info_ = std::move(info);

  parent_viewport_watcher_->GetLayout(
      fit::bind_member(this, &FlatlandSurface::OnGetLayout));
}

void FlatlandSurface::RemoveBufferCollection(FlatlandPixmapId ids) {
  auto iter = pixmap_ids_to_flatland_ids_.find(ids);
  DCHECK(iter != pixmap_ids_to_flatland_ids_.end());
  flatland_.flatland()->ReleaseImage(iter->second.image_id);
  if (iter->second.transform_id.value)
    flatland_.flatland()->ReleaseTransform(iter->second.transform_id);
  pixmap_ids_to_flatland_ids_.erase(iter);
}

void FlatlandSurface::OnPresentComplete(zx_time_t actual_presentation_time) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("viz", "FlatlandSurface::PresentFrame",
                                  TRACE_ID_LOCAL(this), "image_id",
                                  pending_frames_.front().image_id.value);

  auto& frame = pending_frames_.front();

  std::move(frame.completion_callback)
      .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
  std::move(frame.presentation_callback)
      .Run(gfx::PresentationFeedback(
          base::TimeTicks::FromZxTime(actual_presentation_time),
          kDefaultVsyncInterval, gfx::PresentationFeedback::kVSync));

  pending_frames_.pop_front();
}

FlatlandSurface::FlatlandIds FlatlandSurface::CreateOrGetFlatlandIds(
    gfx::NativePixmap* pixmap,
    bool is_primary_plane) {
  const auto& handle =
      static_cast<FlatlandSysmemNativePixmap*>(pixmap)->PeekHandle();
  DCHECK_EQ(handle.buffer_index, 0u);
  DCHECK(handle.buffer_collection_id.has_value());
  const FlatlandPixmapId ids = {
      .buffer_collection_id = handle.buffer_collection_id.value(),
      .buffer_index = handle.buffer_index};

  const auto ids_itr = pixmap_ids_to_flatland_ids_.find(ids);
  if (ids_itr != pixmap_ids_to_flatland_ids_.end()) {
    return ids_itr->second;
  }

  // Create Flatland Image.
  FlatlandSysmemBufferCollection* collection =
      static_cast<FlatlandSysmemNativePixmap*>(pixmap)
          ->sysmem_buffer_collection();
  DCHECK_EQ(collection->id(), ids.buffer_collection_id);
  fuchsia::ui::composition::ImageProperties image_properties;
  image_properties.set_size(GfxSizeToFuchsiaSize(collection->size()));
  const fuchsia::ui::composition::ContentId image_id =
      flatland_.NextContentId();
  flatland_.flatland()->CreateImage(
      image_id, collection->GetFlatlandImportToken(), ids.buffer_index,
      std::move(image_properties));

  // Create flatland transform for overlays.
  fuchsia::ui::composition::TransformId transform_id = {0};
  if (!is_primary_plane) {
    transform_id = flatland_.NextTransformId();
    flatland_.flatland()->CreateTransform(transform_id);
  }

  // Add Flatland ids to |buffer_collection_to_image_id_|.
  FlatlandSurface::FlatlandIds flatland_ids = {.image_id = image_id,
                                               .transform_id = transform_id};
  pixmap_ids_to_flatland_ids_[ids] = flatland_ids;
  collection->AddOnDeletedCallback(
      base::BindOnce(&FlatlandSurface::RemoveBufferCollection,
                     weak_ptr_factory_.GetWeakPtr(), ids));

  return flatland_ids;
}

void FlatlandSurface::ClearScene() {
  for (auto& child : child_transforms_) {
    flatland_.flatland()->RemoveChild(root_transform_id_, child.second);
  }
  child_transforms_.clear();
}

FlatlandSurface::PresentedFrame::PresentedFrame(
    fuchsia::ui::composition::ContentId image_id,
    scoped_refptr<gfx::NativePixmap> primary_plane,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback)
    : image_id(image_id),
      primary_plane(primary_plane),
      completion_callback(std::move(completion_callback)),
      presentation_callback(std::move(presentation_callback)) {}
FlatlandSurface::PresentedFrame::~PresentedFrame() = default;

FlatlandSurface::PresentedFrame::PresentedFrame(PresentedFrame&&) = default;
FlatlandSurface::PresentedFrame& FlatlandSurface::PresentedFrame::operator=(
    PresentedFrame&&) = default;

}  // namespace ui
