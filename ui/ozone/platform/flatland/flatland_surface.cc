// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/zx/eventpair.h>
#include <zircon/types.h>

#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/ozone/platform/flatland/flatland_gpu_host.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"
#include "ui/ozone/public/overlay_plane.h"

namespace ui {

namespace {

std::vector<zx::event> GpuFenceHandlesToZxEvents(
    std::vector<gfx::GpuFenceHandle> handles) {
  std::vector<zx::event> events;
  events.reserve(handles.size());
  for (auto& handle : handles) {
    events.push_back(handle.Release());
  }
  return events;
}

// A struct containing Flatland properties for an associated overlay transform.
// See |OverlayTransformToFlatlandProperties|.
struct OverlayTransformFlatlandProperties {
  fuchsia::math::Vec translation;
  fuchsia::ui::composition::Orientation orientation;
  fuchsia::ui::composition::ImageFlip image_flip;
};

// Converts the overlay transform to the associated Flatland properties. For
// rotation, converts OverlayTransform enum to angle in radians. Since rotation
// occurs around the top-left corner, also returns the associated translation to
// recenter the overlay.
OverlayTransformFlatlandProperties OverlayTransformToFlatlandProperties(
    gfx::OverlayTransform plane_transform,
    gfx::Rect rounded_bounds) {
  switch (plane_transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return {
          .translation = {rounded_bounds.x(), rounded_bounds.y()},
          .orientation = fuchsia::ui::composition::Orientation::CCW_0_DEGREES,
          .image_flip = fuchsia::ui::composition::ImageFlip::NONE};
    // gfx::OverlayTransform and Flatland rotate in opposite directions relative
    // to each other, so swap 90 and 270.
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      return {
          .translation = {rounded_bounds.x() + rounded_bounds.width(),
                          rounded_bounds.y()},
          .orientation = fuchsia::ui::composition::Orientation::CCW_270_DEGREES,
          .image_flip = fuchsia::ui::composition::ImageFlip::NONE};
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      return {
          .translation = {rounded_bounds.x() + rounded_bounds.width(),
                          rounded_bounds.y() + rounded_bounds.height()},
          .orientation = fuchsia::ui::composition::Orientation::CCW_180_DEGREES,
          .image_flip = fuchsia::ui::composition::ImageFlip::NONE};
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      return {
          .translation = {rounded_bounds.x(),
                          rounded_bounds.y() + rounded_bounds.height()},
          .orientation = fuchsia::ui::composition::Orientation::CCW_90_DEGREES,
          .image_flip = fuchsia::ui::composition::ImageFlip::NONE};
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return {
          .translation = {rounded_bounds.x(), rounded_bounds.y()},
          .orientation = fuchsia::ui::composition::Orientation::CCW_0_DEGREES,
          .image_flip = fuchsia::ui::composition::ImageFlip::LEFT_RIGHT};
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return {
          .translation = {rounded_bounds.x(), rounded_bounds.y()},
          .orientation = fuchsia::ui::composition::Orientation::CCW_0_DEGREES,
          .image_flip = fuchsia::ui::composition::ImageFlip::UP_DOWN,
      };
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
    case gfx::OVERLAY_TRANSFORM_INVALID:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return {
      .translation = {rounded_bounds.x(), rounded_bounds.y()},
      .orientation = fuchsia::ui::composition::Orientation::CCW_0_DEGREES,
      .image_flip = fuchsia::ui::composition::ImageFlip::NONE,
  };
}

// Converts a gfx size to the associated Fuchsia size, and accounts for any
// rotation that may be specified in the plane transform (if specified).
fuchsia::math::SizeU GfxSizeToFuchsiaSize(
    const gfx::Size& size,
    gfx::OverlayTransform plane_transform = gfx::OVERLAY_TRANSFORM_NONE) {
  if (plane_transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90 ||
      plane_transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270) {
    return fuchsia::math::SizeU{static_cast<uint32_t>(size.height()),
                                static_cast<uint32_t>(size.width())};
  }
  return fuchsia::math::SizeU{static_cast<uint32_t>(size.width()),
                              static_cast<uint32_t>(size.height())};
}

fuchsia::ui::composition::ContentId CreateImage(
    FlatlandConnection* flatland,
    fuchsia::ui::composition::BufferCollectionImportToken import_token,
    const gfx::Size& size,
    uint32_t vmo_index) {
  fuchsia::ui::composition::ImageProperties image_properties;
  image_properties.set_size(GfxSizeToFuchsiaSize(size));
  const fuchsia::ui::composition::ContentId image_id =
      flatland->NextContentId();
  flatland->flatland()->CreateImage(image_id, std::move(import_token),
                                    vmo_index, std::move(image_properties));
  return image_id;
}

}  // namespace

FlatlandSurface::FlatlandSurface(
    FlatlandSurfaceFactory* flatland_surface_factory,
    gfx::AcceleratedWidget window)
    : flatland_("Chromium FlatlandSurface",
                base::BindOnce(&FlatlandSurface::OnFlatlandError,
                               base::Unretained(this))),
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!logical_size_ || !device_pixel_ratio_) {
    pending_present_closures_.emplace_back(base::BindOnce(
        &FlatlandSurface::Present, base::Unretained(this),
        std::move(primary_plane_pixmap), std::move(overlays),
        std::move(acquire_fences), std::move(release_fences),
        std::move(completion_callback), std::move(presentation_callback)));
    return;
  }

  // Start a new frame by clearing all child transforms.
  ClearScene();

  // Prepare overlay planes.
  for (auto& overlay : overlays) {
    const auto flatland_ids = CreateOrGetFlatlandIds(
        overlay.pixmap.get(), /*is_primary_plane=*/false);
    const auto image_id = flatland_ids.image_id;
    const auto transform_id = flatland_ids.transform_id;
    const auto overlay_plane_transform = absl::get<gfx::OverlayTransform>(
        overlay.overlay_plane_data.plane_transform);

    if (overlay.gpu_fence) {
      acquire_fences.push_back(overlay.gpu_fence->GetGpuFenceHandle().Clone());
    }
    child_transforms_[overlay.overlay_plane_data.z_order] = transform_id;

    const auto rounded_bounds =
        gfx::ToRoundedRect(overlay.overlay_plane_data.display_bounds);

    const auto flatland_properties = OverlayTransformToFlatlandProperties(
        overlay_plane_transform, rounded_bounds);
    flatland_.flatland()->SetOrientation(transform_id,
                                         flatland_properties.orientation);
    flatland_.flatland()->SetTranslation(transform_id,
                                         flatland_properties.translation);
    flatland_.flatland()->SetImageDestinationSize(
        image_id,
        GfxSizeToFuchsiaSize(rounded_bounds.size(), overlay_plane_transform));

    // `crop_rect` is in normalized coordinates, but Flatland expects it to be
    // given in image coordinates.
    gfx::RectF sample_region = overlay.overlay_plane_data.crop_rect;
    sample_region.Intersect(gfx::RectF(1.0, 1.0));
    const gfx::Size& buffer_size = overlay.pixmap->GetBufferSize();
    sample_region.Scale(buffer_size.width(), buffer_size.height());
    flatland_.flatland()->SetImageSampleRegion(
        image_id, {sample_region.x(), sample_region.y(), sample_region.width(),
                   sample_region.height()});
    flatland_.flatland()->SetImageBlendingFunction(
        image_id, overlay.overlay_plane_data.enable_blend
                      ? fuchsia::ui::composition::BlendMode::SRC_OVER
                      : fuchsia::ui::composition::BlendMode::SRC);
    flatland_.flatland()->SetImageOpacity(image_id,
                                          overlay.overlay_plane_data.opacity);
    flatland_.flatland()->SetImageFlip(image_id,
                                       flatland_properties.image_flip);
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
  flatland_.flatland()->SetContent(primary_plane_transform_id_,
                                   primary_plane_image_id);
  // TODO(crbug.com/42050483): We should set SRC blend mode when Chrome has a
  // reliable signal for opaque background.
  flatland_.flatland()->SetImageBlendingFunction(
      primary_plane_image_id, fuchsia::ui::composition::BlendMode::SRC_OVER);

  // Add children in z-order.
  for (auto& child : child_transforms_) {
    flatland_.flatland()->AddChild(root_transform_id_, child.second);
  }

  // We are given the overlays in physical coordinates. Allocation sizes of
  // primary plane buffers may not be equal to |logical_size_| if
  // |device_pixel_ratio_| is applied. Applying a scale at the root converts
  // these back to to the logical coordinates.
  const auto primary_plane_size = primary_plane_pixmap->GetBufferSize();
  const float root_scale =
      static_cast<float>(logical_size_->width()) / primary_plane_size.width();
  DCHECK_EQ(root_scale, static_cast<float>(logical_size_->height()) /
                            primary_plane_size.height());
  DCHECK_EQ(root_scale, 1.f / device_pixel_ratio_.value());
  flatland_.flatland()->SetScale(root_transform_id_, {root_scale, root_scale});

  // Add to pending frame to track callbacks.
  pending_frames_.emplace_back(
      primary_plane_image_id, std::move(primary_plane_pixmap),
      std::move(completion_callback), std::move(presentation_callback));

  // Keep track of release fences from last present for destructor.
  release_fences_from_last_present_.clear();
  for (auto& fence : release_fences) {
    release_fences_from_last_present_.push_back(fence.Clone().Release());
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
  DCHECK(!logical_size_ || pending_present_closures_.empty());

  logical_size_ =
      gfx::Size(info.logical_size().width, info.logical_size().height);
  DCHECK_EQ(info.device_pixel_ratio().x, info.device_pixel_ratio().y);
  DCHECK_GT(info.device_pixel_ratio().x, 0.f);
  device_pixel_ratio_ = info.device_pixel_ratio().x;

  // Run |pending_present_closures_| that are waiting on |logical_size_| and
  // |device_pixel_ratio_|.
  for (auto& closure : pending_present_closures_) {
    std::move(closure).Run();
  }
  pending_present_closures_.clear();

  parent_viewport_watcher_->GetLayout(
      fit::bind_member(this, &FlatlandSurface::OnGetLayout));
}

void FlatlandSurface::RemovePixmapResources(FlatlandPixmapId ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto iter = pixmap_ids_to_flatland_ids_.find(ids);
  CHECK(iter != pixmap_ids_to_flatland_ids_.end(), base::NotFatalUntil::M130);
  flatland_.flatland()->ReleaseImage(iter->second.image_id);
  if (iter->second.transform_id.value) {
    flatland_.flatland()->ReleaseTransform(iter->second.transform_id);
  }
  pixmap_ids_to_flatland_ids_.erase(iter);
}

void FlatlandSurface::OnPresentComplete(
    base::TimeTicks actual_presentation_time,
    base::TimeDelta presentation_interval) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("viz", "FlatlandSurface::PresentFrame",
                                  TRACE_ID_LOCAL(this), "image_id",
                                  pending_frames_.front().image_id.value);

  auto& frame = pending_frames_.front();

  std::move(frame.completion_callback)
      .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
  std::move(frame.presentation_callback)
      .Run(gfx::PresentationFeedback(actual_presentation_time,
                                     presentation_interval,
                                     gfx::PresentationFeedback::kVSync));

  pending_frames_.pop_front();
}

FlatlandSurface::FlatlandIds FlatlandSurface::CreateOrGetFlatlandIds(
    gfx::NativePixmap* pixmap,
    bool is_primary_plane) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const auto& handle =
      static_cast<FlatlandSysmemNativePixmap*>(pixmap)->PeekHandle();
  FlatlandSysmemBufferCollection* collection =
      static_cast<FlatlandSysmemNativePixmap*>(pixmap)
          ->sysmem_buffer_collection();
  zx_koid_t buffer_collection_id = collection->id();
  const FlatlandPixmapId ids = {.buffer_collection_id = buffer_collection_id,
                                .buffer_index = handle.buffer_index};

  const auto ids_itr = pixmap_ids_to_flatland_ids_.find(ids);
  if (ids_itr != pixmap_ids_to_flatland_ids_.end()) {
    // There is a size change in pixmap, we should recreate the image with the
    // updated size.
    if (ids_itr->second.image_size != pixmap->GetBufferSize()) {
      flatland_.flatland()->ReleaseImage(ids_itr->second.image_id);
      ids_itr->second.image_id =
          CreateImage(&flatland_, collection->GetFlatlandImportToken(),
                      pixmap->GetBufferSize(), ids.buffer_index);
      ids_itr->second.image_size = pixmap->GetBufferSize();
      if (!is_primary_plane) {
        flatland_.flatland()->SetContent(ids_itr->second.transform_id,
                                         ids_itr->second.image_id);
      }
    }
    return ids_itr->second;
  }

  const fuchsia::ui::composition::ContentId image_id =
      CreateImage(&flatland_, collection->GetFlatlandImportToken(),
                  pixmap->GetBufferSize(), ids.buffer_index);

  // Skip creating a transform for the primary plane because
  // |primary_plane_transform_id_| is used.
  fuchsia::ui::composition::TransformId transform_id = {0};
  if (!is_primary_plane) {
    transform_id = flatland_.NextTransformId();
    flatland_.flatland()->CreateTransform(transform_id);
    flatland_.flatland()->SetContent(transform_id, image_id);
  }

  FlatlandSurface::FlatlandIds flatland_ids = {
      .image_id = image_id,
      .transform_id = transform_id,
      .image_size = (pixmap->GetBufferSize())};
  pixmap_ids_to_flatland_ids_[ids] = flatland_ids;
  collection->AddOnReleasedCallback(
      base::BindOnce(&FlatlandSurface::RemovePixmapResources,
                     weak_ptr_factory_.GetWeakPtr(), ids));

  return flatland_ids;
}

void FlatlandSurface::ClearScene() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto& child : child_transforms_) {
    flatland_.flatland()->RemoveChild(root_transform_id_, child.second);
  }
  child_transforms_.clear();
}

void FlatlandSurface::OnFlatlandError(
    fuchsia::ui::composition::FlatlandError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(ERROR) << "Flatland error: " << static_cast<int>(error);
  base::LogFidlErrorAndExitProcess(FROM_HERE,
                                   "fuchsia::ui::composition::Flatland");
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
