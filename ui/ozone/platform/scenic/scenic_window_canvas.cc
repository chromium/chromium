// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window_canvas.h"

#include <memory>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/scenic/scenic_window.h"

namespace ui {

// How long we want to wait for release-fence from scenic for previous frames.
constexpr base::TimeDelta kFrameReleaseTimeout =
    base::TimeDelta::FromMilliseconds(500);

ScenicWindowCanvas::Frame::Frame() = default;
ScenicWindowCanvas::Frame::~Frame() = default;

void ScenicWindowCanvas::Frame::Initialize(gfx::Size size,
                                           scenic::Session* scenic) {
  size_t bytes_per_row =
      size.width() * SkColorTypeBytesPerPixel(kN32_SkColorType);
  size_t buffer_size = bytes_per_row * size.height();

  base::WritableSharedMemoryRegion memory_region =
      base::WritableSharedMemoryRegion::Create(buffer_size);
  memory_mapping = memory_region.Map();

  if (!memory_mapping.IsValid()) {
    LOG(WARNING) << "Failed to map memory for ScenicWindowCanvas.";
    memory_mapping = base::WritableSharedMemoryMapping();
    surface.reset();
    return;
  }

  auto read_only_memory = base::WritableSharedMemoryRegion::ConvertToReadOnly(
      std::move(memory_region));
  auto memory_handle =
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(read_only_memory));
  scenic_memory = std::make_unique<scenic::Memory>(
      scenic, memory_handle.PassPlatformHandle(), buffer_size,
      fuchsia::images::MemoryType::HOST_MEMORY);
  surface = SkSurface::MakeRasterDirect(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      memory_mapping.memory(), bytes_per_row);
  dirty_region.setRect(gfx::RectToSkIRect(gfx::Rect(size)));
}

void ScenicWindowCanvas::Frame::CopyDirtyRegionFrom(const Frame& frame) {
  int stride = surface->width() * SkColorTypeBytesPerPixel(kN32_SkColorType);
  for (SkRegion::Iterator i(dirty_region); !i.done(); i.next()) {
    uint8_t* dst_ptr =
        static_cast<uint8_t*>(memory_mapping.memory()) +
        i.rect().x() * SkColorTypeBytesPerPixel(kN32_SkColorType) +
        i.rect().y() * stride;
    frame.surface->readPixels(
        SkImageInfo::MakeN32Premul(i.rect().width(), i.rect().height()),
        dst_ptr, stride, i.rect().x(), i.rect().y());
  }
  dirty_region.setEmpty();
}

ScenicWindowCanvas::ScenicWindowCanvas(ScenicSurface* scenic_surface)
    : scenic_surface_(scenic_surface) {}

ScenicWindowCanvas::~ScenicWindowCanvas() = default;

void ScenicWindowCanvas::ResizeCanvas(const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
  viewport_size_.SetToMax(gfx::Size(1, 1));

  // Allocate new buffers with the new size.
  for (int i = 0; i < kNumBuffers; ++i) {
    frames_[i].Initialize(viewport_size_, scenic_surface_->scenic_session());
  }
}

SkCanvas* ScenicWindowCanvas::GetCanvas() {
  if (viewport_size_.IsEmpty() || frames_[current_frame_].is_empty())
    return nullptr;

  // Wait for the buffer to become available. This call has to be blocking
  // because GetSurface() and PresentCanvas() are synchronous.
  //
  // TODO(sergeyu): Consider updating SurfaceOzoneCanvas interface to allow
  // asynchronous PresentCanvas() and then wait for release-fence before
  // PresentCanvas() completes.
  if (frames_[current_frame_].release_fence) {
    auto status = frames_[current_frame_].release_fence.wait_one(
        ZX_EVENT_SIGNALED,
        zx::deadline_after(zx::duration(kFrameReleaseTimeout.InNanoseconds())),
        nullptr);
    if (status == ZX_ERR_TIMED_OUT) {
      // Timeout here indicates that Scenic is most likely broken. If it still
      // works, then in the worst case returning before |release_fence| is
      // signaled will cause screen tearing.
      LOG(WARNING) << "Release fence from previous frame timed out after 500ms";
    } else {
      ZX_CHECK(status == ZX_OK, status);
    }
  }

  return frames_[current_frame_].surface->getCanvas();
}

void ScenicWindowCanvas::PresentCanvas(const gfx::Rect& damage) {
  // Subtract |damage| from the dirty region in the current frame since it's
  // been repainted.
  SkIRect sk_damage = gfx::RectToSkIRect(damage);
  frames_[current_frame_].dirty_region.op(sk_damage, SkRegion::kDifference_Op);

  // Copy dirty region from the previous buffer to make sure the whole frame
  // is up to date.
  int prev_frame =
      current_frame_ == 0 ? (kNumBuffers - 1) : (current_frame_ - 1);
  frames_[current_frame_].CopyDirtyRegionFrom(frames_[prev_frame]);

  // |damage| rect was updated in the current frame. It means that the rect is
  // no longer valid in all other buffers. Add |damage| to |dirty_region| in all
  // buffers except the current one.
  for (int i = 0; i < kNumBuffers; ++i) {
    if (i != current_frame_) {
      frames_[i].dirty_region.op(sk_damage, SkRegion::kUnion_Op);
    }
  }

  // Create image that wraps the buffer and attach it as texture for the node.
  fuchsia::images::ImageInfo info;
  info.width = viewport_size_.width();
  info.height = viewport_size_.height();
  info.stride =
      viewport_size_.width() * SkColorTypeBytesPerPixel(kN32_SkColorType);
  scenic::Image image(*frames_[current_frame_].scenic_memory, 0,
                      std::move(info));
  // TODO(spang): Consider using ImagePipe for consistency with vulkan path.
  scenic_surface_->SetTextureToImage(image);

  // Create release fence for the current buffer or reset it if it already
  // exists.
  if (!frames_[current_frame_].release_fence) {
    auto status = zx::event::create(
        /*options=*/0u, &(frames_[current_frame_].release_fence));
    ZX_CHECK(status == ZX_OK, status);
  } else {
    auto status = frames_[current_frame_].release_fence.signal(
        /*clear_mask=*/ZX_EVENT_SIGNALED, /*set_maks=*/0);
    ZX_CHECK(status == ZX_OK, status);
  }

  // Add release-fence for the Present2() call below. The fence is used in
  // GetCanvas() to ensure that we reuse the buffer only after it's released
  // from scenic.
  zx::event release_fence_dup;
  auto status = frames_[current_frame_].release_fence.duplicate(
      ZX_RIGHT_SAME_RIGHTS, &release_fence_dup);
  ZX_CHECK(status == ZX_OK, status);
  scenic_surface_->scenic_session()->EnqueueReleaseFence(
      std::move(release_fence_dup));
  scenic_surface_->scenic_session()->Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});

  // Move to the next buffer.
  current_frame_ = (current_frame_ + 1) % kNumBuffers;
}

std::unique_ptr<gfx::VSyncProvider> ScenicWindowCanvas::CreateVSyncProvider() {
  // TODO(crbug.com/829980): Implement VSyncProvider. It can be implemented by
  // observing FuturePresentationTimes returned from
  // scenic::Session::Present2().
  return nullptr;
}

}  // namespace ui
