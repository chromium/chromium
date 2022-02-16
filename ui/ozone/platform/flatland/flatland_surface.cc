// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/zx/eventpair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/ozone/platform/flatland/flatland_gpu_host.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"

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
  flatland_.flatland()->AddChild(root_transform_id_,
                                 primary_plane_transform_id_);

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
  auto& handle =
      static_cast<FlatlandSysmemNativePixmap*>(primary_plane_pixmap.get())
          ->PeekHandle();
  DCHECK_EQ(handle.buffer_index, 0u);

  // Check |buffer_collection_to_image_id_| to see if there is a Flatland Image
  // created for this pixmap.
  if (!buffer_collection_to_image_id_.contains(handle.buffer_collection_id)) {
    FlatlandSysmemBufferCollection* collection =
        static_cast<FlatlandSysmemNativePixmap*>(primary_plane_pixmap.get())
            ->sysmem_buffer_collection();
    DCHECK_EQ(collection->id(), *(handle.buffer_collection_id));

    // Create Flatland Image.
    const fuchsia::math::SizeU size = {
        static_cast<uint32_t>(collection->size().width()),
        static_cast<uint32_t>(collection->size().height())};
    DCHECK_EQ(size.width, layout_info_.logical_size().width);
    DCHECK_EQ(size.height, layout_info_.logical_size().height);
    fuchsia::ui::composition::ImageProperties image_properties;
    image_properties.set_size(size);
    auto image_id = flatland_.NextContentId();
    flatland_.flatland()->CreateImage(
        image_id, collection->GetFlatlandImportToken(), handle.buffer_index,
        std::move(image_properties));
    flatland_.flatland()->SetImageDestinationSize(image_id, size);
    // Set main layer to be opaque.
    flatland_.flatland()->SetImageBlendingFunction(
        image_id, fuchsia::ui::composition::BlendMode::SRC);

    // Add Flatland Image to |buffer_collection_to_image_id_|.
    buffer_collection_to_image_id_[collection->id()] = image_id;
    collection->AddOnDeletedCallback(
        base::BindOnce(&FlatlandSurface::RemoveBufferCollection,
                       weak_ptr_factory_.GetWeakPtr(), collection->id()));
  }

  const auto image_id =
      buffer_collection_to_image_id_.at(handle.buffer_collection_id);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("viz", "FlatlandSurface::Present",
                                    TRACE_ID_LOCAL(this), "image_id",
                                    image_id.value);

  // Present overlays.
  for (auto& overlay : overlays) {
    overlay.pixmap->ScheduleOverlayPlane(window_, overlay.overlay_plane_data,
                                         /*acquire_fences=*/{},
                                         /*release_fences=*/{});
  }

  // Add to pending frame to track callbacks.
  pending_frames_.emplace_back(image_id, std::move(primary_plane_pixmap),
                               std::move(completion_callback),
                               std::move(presentation_callback));

  // Keep track of release fences from last present for destructor.
  release_fences_from_last_present_.clear();
  for (auto& fence : release_fences) {
    release_fences_from_last_present_.push_back(
        DuplicateZxEvent(fence.owned_event));
  }

  // Update primary plane transform and Present to Flatland.
  flatland_.flatland()->SetContent(primary_plane_transform_id_, image_id);
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

void FlatlandSurface::RemoveBufferCollection(
    gfx::SysmemBufferCollectionId buffer_collection_id) {
  auto iter = buffer_collection_to_image_id_.find(buffer_collection_id);
  DCHECK(iter != buffer_collection_to_image_id_.end());

  flatland_.flatland()->ReleaseImage(iter->second);
  buffer_collection_to_image_id_.erase(iter);
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
