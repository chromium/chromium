// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/eventpair.h>
#include <zircon/types.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/process_context.h"
#include "base/numerics/math_constants.h"
#include "base/process/process_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "sysmem_native_pixmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/scenic/scenic_gpu_host.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

namespace ui {

namespace {

// Scenic has z-fighting problems in 3D API, so we add tiny z plane increments
// to elevate content. ViewProperties set by ScenicWindow sets z-plane to
// [-0.5f, 0.5f] range, so 0.01f is small enough to make a difference.
constexpr float kElevationStep = 0.01f;

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

std::vector<gfx::GpuFence> DuplicateGpuFences(
    const std::vector<gfx::GpuFenceHandle>& fence_handles) {
  std::vector<gfx::GpuFence> fences;
  fences.reserve(fence_handles.size());
  for (const auto& handle : fence_handles)
    fences.emplace_back(handle.Clone());
  return fences;
}

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
    SysmemBufferManager* sysmem_buffer_manager,
    gfx::AcceleratedWidget window,
    scenic::SessionPtrAndListenerRequest sesion_and_listener_request)
    : scenic_session_(std::move(sesion_and_listener_request)),
      safe_presenter_(&scenic_session_),
      main_shape_(&scenic_session_),
      main_material_(&scenic_session_),
      scenic_surface_factory_(scenic_surface_factory),
      sysmem_buffer_manager_(sysmem_buffer_manager),
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

  // Signal release fences that were submitted in the last PresentImage(). This
  // is necessary because ExternalVkImageBacking destructor will wait for the
  // corresponding semaphores, while they may not be signaled by the ImagePipe.
  for (auto& fence : release_fences_from_last_present_) {
    auto status =
        fence.signal(/*clear_mask=*/0, /*set_mask=*/ZX_EVENT_SIGNALED);
    ZX_DCHECK(status == ZX_OK, status);
  }

  scenic_surface_factory_->RemoveSurface(window_);
}

ScenicSurface::OverlayViewInfo::OverlayViewInfo(
    scenic::Session* scenic_session,
    fuchsia::ui::views::ViewHolderToken view_holder_token)
    : view_holder(scenic_session,
                  std::move(view_holder_token),
                  "OverlayViewHolder"),
      entity_node(scenic_session) {
  view_holder.SetHitTestBehavior(fuchsia::ui::gfx::HitTestBehavior::kSuppress);
  entity_node.AddChild(view_holder);
}

void ScenicSurface::OnScenicEvents(
    std::vector<fuchsia::ui::scenic::Event> events) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& event : events) {
    DCHECK(event.is_gfx());
    switch (event.gfx().Which()) {
      case fuchsia::ui::gfx::Event::kMetrics: {
        DCHECK_EQ(event.gfx().metrics().node_id, main_shape_.id());
        // This is enough to track size because |main_shape_| is 1x1.
        const auto& metrics = event.gfx().metrics().metrics;
        main_shape_size_.set_width(metrics.scale_x);
        main_shape_size_.set_height(metrics.scale_y);

        // Update layout only if Present() was called at least once, i.e. we
        // have a frame to display.
        if (last_frame_present_time_ != base::TimeTicks())
          UpdateViewHolderScene();
        break;
      }
      case fuchsia::ui::gfx::Event::kViewDetachedFromScene: {
        DCHECK(event.gfx().view_detached_from_scene().view_id == parent_->id());
        // Present an empty frame to ensure that the outdated content doesn't
        // become visible if the view is attached again.
        PresentEmptyImage();
        break;
      }
      default:
        break;
    }
  }
}

void ScenicSurface::Present(
    scoped_refptr<gfx::NativePixmap> primary_plane_pixmap,
    std::vector<ui::OverlayPlane> overlays_to_present,
    std::vector<gfx::GpuFenceHandle> acquire_fences,
    std::vector<gfx::GpuFenceHandle> release_fences,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  if (!image_pipe_) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_SKIPPED));
    return;
  }

  auto& handle = static_cast<SysmemNativePixmap*>(primary_plane_pixmap.get())
                     ->PeekHandle();
  DCHECK_EQ(handle.buffer_index, 0u);
  zx_koid_t buffer_collection_id =
      base::GetKoid(handle.buffer_collection_handle).value();
  DCHECK(buffer_collection_to_image_id_.contains(buffer_collection_id));
  uint32_t image_id = buffer_collection_to_image_id_.at(buffer_collection_id);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("viz", "ScenicSurface::PresentFrame",
                                    TRACE_ID_LOCAL(this), "image_id", image_id);

  // Always update layout for the first frame.
  bool layout_update_required = last_frame_present_time_ == base::TimeTicks();

  for (auto& overlay_view : overlay_views_) {
    overlay_view.second.should_be_visible = false;
  }

  for (auto& overlay : overlays_to_present) {
    overlay.pixmap->ScheduleOverlayPlane(window_, overlay.overlay_plane_data,
                                         DuplicateGpuFences(acquire_fences),
                                         /*release_fences=*/{});

    auto* pixmap = static_cast<SysmemNativePixmap*>(overlay.pixmap.get());
    auto& overlay_handle = pixmap->PeekHandle();
    zx_koid_t overlay_id =
        base::GetKoid(overlay_handle.buffer_collection_handle).value();
    auto it = overlay_views_.find(overlay_id);

    // If this is a new overlay then attach it.
    if (it == overlay_views_.end()) {
      auto token_pair = scenic::ViewTokenPair::New();
      pixmap->GetScenicOverlayView()->AttachToScenicSurface(
          std::move(token_pair.view_token));
      auto emplace_result = overlay_views_.emplace(
          std::piecewise_construct, std::forward_as_tuple(overlay_id),
          std::forward_as_tuple(&scenic_session_,
                                std::move(token_pair.view_holder_token)));
      it = emplace_result.first;
      layout_update_required = true;
    }

    auto& overlay_view_info = it->second;
    overlay_view_info.should_be_visible = true;

    auto& overlay_data = overlay.overlay_plane_data;
    auto rounded_bounds = gfx::ToRoundedRect(overlay_data.display_bounds);
    auto transform =
        absl::get<gfx::OverlayTransform>(overlay_data.plane_transform);
    if (overlay_view_info.plane_z_order != overlay_data.z_order ||
        overlay_view_info.display_bounds != rounded_bounds ||
        overlay_view_info.crop_rect != overlay_data.crop_rect ||
        overlay_view_info.plane_transform != transform) {
      overlay_view_info.plane_z_order = overlay_data.z_order;
      overlay_view_info.display_bounds = rounded_bounds;
      overlay_view_info.crop_rect = overlay_data.crop_rect;
      overlay_view_info.plane_transform = transform;
      layout_update_required = true;
    }
  }

  // Hide all overlays views that are not in `overlays_to_present`.
  auto it = overlay_views_.begin();
  while (it != overlay_views_.end()) {
    auto& overlay_view = it->second;
    if (!overlay_view.should_be_visible) {
      layout_update_required = true;
      parent_->DetachChild(overlay_view.entity_node);
      it = overlay_views_.erase(it);
      continue;
    }

    it++;
  }

  if (layout_update_required) {
    for (auto& fence : acquire_fences) {
      scenic_session_.EnqueueAcquireFence(std::move(fence.Clone().owned_event));
    }
    UpdateViewHolderScene();
  }

  pending_frames_.emplace_back(
      next_frame_ordinal_++, image_id, std::move(primary_plane_pixmap),
      std::move(completion_callback), std::move(presentation_callback));

  auto now = base::TimeTicks::Now();

  auto present_time = now;

  // If we have PresentationState frame a previously displayed frame then use it
  // to calculate target timestamp for the new frame.
  if (presentation_state_) {
    uint32_t relative_position = pending_frames_.back().ordinal -
                                 presentation_state_->presented_frame_ordinal;
    present_time = presentation_state_->presentation_time +
                   presentation_state_->interval * relative_position -
                   base::Milliseconds(1);
    present_time = std::max(present_time, now);
  }

  // Ensure that the target timestamp is not decreasing from the previous frame,
  // since Scenic doesn't allow it (see crbug.com/1181528).
  present_time = std::max(present_time, last_frame_present_time_);
  last_frame_present_time_ = present_time;

  release_fences_from_last_present_.clear();
  for (auto& fence : release_fences) {
    release_fences_from_last_present_.push_back(
        DuplicateZxEvent(fence.owned_event));
  }

  image_pipe_->PresentImage(
      image_id, present_time.ToZxTime(),
      GpuFenceHandlesToZxEvents(std::move(acquire_fences)),
      GpuFenceHandlesToZxEvents(std::move(release_fences)),
      fit::bind_member(this, &ScenicSurface::OnPresentComplete));
}

scoped_refptr<gfx::NativePixmap> ScenicSurface::AllocatePrimaryPlanePixmap(
    VkDevice vk_device,
    const gfx::Size& size,
    gfx::BufferFormat buffer_format) {
  if (!image_pipe_)
    InitializeImagePipe();

  // Create buffer collection with 2 extra tokens: one for Vulkan and one for
  // the ImagePipe.
  fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token;
  zx_status_t status =
      sysmem_buffer_manager_->GetAllocator()->AllocateSharedCollection(
          collection_token.NewRequest());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status)
        << "fuchsia.sysmem.Allocator.AllocateSharedCollection()";
    return {};
  }

  collection_token->SetName(100u, "ChromiumPrimaryPlaneOutput");
  collection_token->SetDebugClientInfo("vulkan", 0u);

  fuchsia::sysmem::BufferCollectionTokenSyncPtr token_for_scenic;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              token_for_scenic.NewRequest());
  token_for_scenic->SetDebugClientInfo("scenic", 0u);

  // Register the new buffer collection with the ImagePipe. Since there will
  // only be a single buffer in the buffer collection we use the same value for
  // both buffer collection id and image ids.
  const uint32_t image_id = ++next_unique_id_;
  image_pipe_->AddBufferCollection(image_id, std::move(token_for_scenic));

  // Register the new buffer collection with sysmem.
  gfx::NativePixmapHandle pixmap_handle;
  zx::eventpair service_handle;
  status = zx::eventpair::create(0, &pixmap_handle.buffer_collection_handle,
                                 &service_handle);
  ZX_DCHECK(status == ZX_OK, status);

  zx_koid_t buffer_collection_id =
      base::GetKoid(pixmap_handle.buffer_collection_handle).value();
  buffer_collection_to_image_id_[buffer_collection_id] = image_id;

  auto buffer_collection = sysmem_buffer_manager_->ImportSysmemBufferCollection(
      vk_device, std::move(service_handle),
      collection_token.Unbind().TakeChannel(), size, buffer_format,
      gfx::BufferUsage::SCANOUT, 1,
      /*register_with_image_pipe=*/false);

  if (!buffer_collection) {
    ZX_DLOG(ERROR, status) << "Failed to allocate sysmem buffer collection";
    return {};
  }

  buffer_collection->AddOnReleasedCallback(
      base::BindOnce(&ScenicSurface::RemoveBufferCollection,
                     weak_ptr_factory_.GetWeakPtr(), buffer_collection_id));

  fuchsia::sysmem::ImageFormat_2 image_format;
  image_format.coded_width = size.width();
  image_format.coded_height = size.height();
  image_pipe_->AddImage(image_id, image_id, 0, image_format);

  return buffer_collection->CreateNativePixmap(std::move(pixmap_handle), size);
}

void ScenicSurface::SetTextureToNewImagePipe(
    fidl::InterfaceRequest<fuchsia::images::ImagePipe2> image_pipe_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint32_t image_pipe_id = scenic_session_.AllocResourceId();
  scenic_session_.Enqueue(scenic::NewCreateImagePipe2Cmd(
      image_pipe_id, std::move(image_pipe_request)));
  main_material_.SetTexture(image_pipe_id);
  main_shape_.SetMaterial(main_material_);
  scenic_session_.ReleaseResource(image_pipe_id);
  safe_presenter_.QueuePresent();
}

void ScenicSurface::SetTextureToImage(const scenic::Image& image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  main_material_.SetTexture(image);
  main_shape_.SetMaterial(main_material_);
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

  // Defer first Present call to SetTextureToNewImagePipe().
  return mojo::PlatformHandle(std::move(tokens.view_holder_token.value));
}

void ScenicSurface::InitializeImagePipe() {
  DCHECK(!image_pipe_);

  SetTextureToNewImagePipe(image_pipe_.NewRequest());

  image_pipe_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ImagePipe disconnected";

    for (auto& frame : pending_frames_) {
      std::move(frame.completion_callback)
          .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    }
    pending_frames_.clear();
  });
}

void ScenicSurface::RemoveBufferCollection(zx_koid_t buffer_collection_id) {
  DCHECK(image_pipe_);

  auto iter = buffer_collection_to_image_id_.find(buffer_collection_id);
  DCHECK(iter != buffer_collection_to_image_id_.end());

  image_pipe_->RemoveBufferCollection(iter->second);
  buffer_collection_to_image_id_.erase(iter);
}

void ScenicSurface::OnPresentComplete(
    fuchsia::images::PresentationInfo presentation_info) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("viz", "ScenicSurface::PresentFrame",
                                  TRACE_ID_LOCAL(this), "image_id",
                                  pending_frames_.front().image_id);

  auto presentation_time =
      base::TimeTicks::FromZxTime(presentation_info.presentation_time);
  auto presentation_interval =
      base::TimeDelta::FromZxDuration(presentation_info.presentation_interval);

  auto& frame = pending_frames_.front();

  std::move(frame.completion_callback)
      .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
  std::move(frame.presentation_callback)
      .Run(gfx::PresentationFeedback(presentation_time, presentation_interval,
                                     gfx::PresentationFeedback::kVSync));

  presentation_state_ =
      PresentationState{static_cast<int>(frame.ordinal), presentation_time,
                        presentation_interval};

  pending_frames_.pop_front();
}

void ScenicSurface::UpdateViewHolderScene() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Layout will be updated once we receive view size.
  if (main_shape_size_.IsEmpty())
    return;

  // |plane_z_order| for main surface is 0.
  int min_z_order = 0;
  for (auto& item : overlay_views_) {
    auto& overlay_view = item.second;
    min_z_order = std::min(overlay_view.plane_z_order, min_z_order);
  }

  for (auto& item : overlay_views_) {
    auto& overlay_view = item.second;

    // No-op if the node is already attached.
    parent_->AddChild(overlay_view.entity_node);

    // Apply view bound clipping around the ImagePipe that has size 1x1 and
    // centered at (0, 0).
    fuchsia::ui::gfx::ViewProperties view_properties;
    const float left_bound = -0.5f + overlay_view.crop_rect.x();
    const float top_bound = -0.5f + overlay_view.crop_rect.y();
    view_properties.bounding_box = {
        {left_bound, top_bound, 0.f},
        {left_bound + overlay_view.crop_rect.width(),
         top_bound + overlay_view.crop_rect.height(), 0.f}};
    view_properties.focus_change = false;
    overlay_view.view_holder.SetViewProperties(std::move(view_properties));

    // We receive |display_bounds| in screen coordinates. Convert them to fit
    // 1x1 View, which is later scaled up by the browser process.
    float scaled_width =
        overlay_view.display_bounds.width() /
        (overlay_view.crop_rect.width() * main_shape_size_.width());
    float scaled_height =
        overlay_view.display_bounds.height() /
        (overlay_view.crop_rect.height() * main_shape_size_.height());
    const float scaled_x =
        overlay_view.display_bounds.x() / main_shape_size_.width();
    const float scaled_y =
        overlay_view.display_bounds.y() / main_shape_size_.height();

    // Position ImagePipe based on the display bounds given.
    overlay_view.entity_node.SetTranslation(
        -0.5f + scaled_x + scaled_width / 2,
        -0.5f + scaled_y + scaled_height / 2,
        (min_z_order - overlay_view.plane_z_order) * kElevationStep);

    // Apply rotation if given. Scenic expects rotation passed as Quaternion.
    const float angle = OverlayTransformToRadians(overlay_view.plane_transform);
    overlay_view.entity_node.SetRotation(
        {0.f, 0.f, sinf(angle * .5f), cosf(angle * .5f)});

    // Scenic applies scaling before rotation.
    if (overlay_view.plane_transform == gfx::OVERLAY_TRANSFORM_ROTATE_90 ||
        overlay_view.plane_transform == gfx::OVERLAY_TRANSFORM_ROTATE_270) {
      std::swap(scaled_width, scaled_height);
    }

    // Scenic expects flip as negative scaling.
    if (overlay_view.plane_transform ==
        gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL) {
      scaled_width = -scaled_width;
    } else if (overlay_view.plane_transform ==
               gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL) {
      scaled_height = -scaled_height;
    }

    // Scale ImagePipe based on the display bounds and clip rect given.
    overlay_view.entity_node.SetScale(scaled_width, scaled_height, 1.f);
  }

  main_material_.SetColor(255, 255, 255, 0 > min_z_order ? 254 : 255);
  main_shape_.SetTranslation(0.f, 0.f, min_z_order * kElevationStep);

  safe_presenter_.QueuePresent();
}

void ScenicSurface::PresentEmptyImage() {
  if (last_frame_present_time_ == base::TimeTicks())
    return;

  fuchsia::sysmem::BufferCollectionTokenSyncPtr dummy_collection_token;
  zx_status_t status =
      sysmem_buffer_manager_->GetAllocator()->AllocateSharedCollection(
          dummy_collection_token.NewRequest());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status)
        << "fuchsia.sysmem.Allocator.AllocateSharedCollection()";
    return;
  }
  dummy_collection_token->SetName(100u, "DummyImageCollection");
  dummy_collection_token->SetDebugClientInfo("chromium", 0u);

  fuchsia::sysmem::BufferCollectionTokenSyncPtr token_for_scenic;
  dummy_collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                                    token_for_scenic.NewRequest());
  token_for_scenic->SetDebugClientInfo("scenic", 0u);

  const uint32_t image_id = ++next_unique_id_;
  image_pipe_->AddBufferCollection(image_id, std::move(token_for_scenic));

  // Synchroniously wait for the collection to be allocated before proceeding.
  fuchsia::sysmem::BufferCollectionSyncPtr dummy_collection;
  status = sysmem_buffer_manager_->GetAllocator()->BindSharedCollection(
      std::move(dummy_collection_token), dummy_collection.NewRequest());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.Allocator.BindSharedCollection()";
    return;
  }

  fuchsia::sysmem::BufferCollectionConstraints constraints;
  constraints.usage.none = fuchsia::sysmem::noneUsage;
  constraints.min_buffer_count = 1;
  constraints.image_format_constraints_count = 0;
  status = dummy_collection->SetConstraints(/*has_constraints=*/true,
                                            std::move(constraints));
  zx_status_t wait_status;
  fuchsia::sysmem::BufferCollectionInfo_2 buffers_info;
  status =
      dummy_collection->WaitForBuffersAllocated(&wait_status, &buffers_info);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection failed";
    return;
  }
  dummy_collection->Close();

  // Present the first image from the collection.
  fuchsia::sysmem::ImageFormat_2 image_format;
  image_format.coded_width = 1;
  image_format.coded_height = 1;
  image_pipe_->AddImage(image_id, image_id, 0, image_format);

  image_pipe_->PresentImage(image_id, last_frame_present_time_.ToZxTime(), {},
                            {}, [](fuchsia::images::PresentationInfo) {});
  image_pipe_->RemoveBufferCollection(image_id);
}

ScenicSurface::PresentedFrame::PresentedFrame(
    uint32_t ordinal,
    uint32_t image_id,
    scoped_refptr<gfx::NativePixmap> primary_plane,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback)
    : ordinal(ordinal),
      image_id(image_id),
      primary_plane(std::move(primary_plane)),
      completion_callback(std::move(completion_callback)),
      presentation_callback(std::move(presentation_callback)) {}
ScenicSurface::PresentedFrame::~PresentedFrame() = default;

ScenicSurface::PresentedFrame::PresentedFrame(PresentedFrame&&) = default;
ScenicSurface::PresentedFrame& ScenicSurface::PresentedFrame::operator=(
    PresentedFrame&&) = default;

}  // namespace ui
