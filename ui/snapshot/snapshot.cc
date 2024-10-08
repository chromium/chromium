// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_util.h"

namespace ui {

namespace {

scoped_refptr<base::RefCountedMemory> EncodeImageAsPNG(
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    return nullptr;
  }
  DCHECK(!image.AsImageSkia().GetRepresentation(1.0f).is_null());

  return image.As1xPNGBytes();
}

scoped_refptr<base::RefCountedMemory> EncodeImageAsJPEG(
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    return nullptr;
  }
  DCHECK(!image.AsImageSkia().GetRepresentation(1.0f).is_null());

  std::optional<std::vector<uint8_t>> result =
      gfx::JPEG1xEncodedDataFromImage(image, /*quality=*/100);
  if (!result.has_value()) {
    return nullptr;
  }
  return base::RefCountedBytes::TakeVector(&result.value());
}

void EncodeImageAndScheduleCallback(
    scoped_refptr<base::RefCountedMemory> (*encode_func)(const gfx::Image&),
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory> data)>
        callback,
    gfx::Image image) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(encode_func, std::move(image)), std::move(callback));
}

}  // namespace

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)

// Note that Android and Aura versions of this function are in
// snapshot_android.cc and snapshot_aura.cc respectively.

void GrabWindowSnapshotAndScale(gfx::NativeWindow window,
                                const gfx::Rect& source_rect,
                                const gfx::Size& target_size,
                                GrabSnapshotImageCallback callback) {
  auto resize_image = [](const gfx::Size& target_size,
                         GrabSnapshotImageCallback callback, gfx::Image image) {
    if (image.IsEmpty()) {
      std::move(callback).Run(image);
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(gfx::ResizedImage, std::move(image), target_size),
        std::move(callback));
  };

  GrabWindowSnapshot(
      window, source_rect,
      base::BindOnce(resize_image, target_size, std::move(callback)));
}

#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)

void GrabWindowSnapshotAsPNG(gfx::NativeWindow window,
                             const gfx::Rect& source_rect,
                             GrabSnapshotDataCallback callback) {
  GrabWindowSnapshot(window, source_rect,
                     base::BindOnce(&EncodeImageAndScheduleCallback,
                                    &EncodeImageAsPNG, std::move(callback)));
}

void GrabWindowSnapshotAsJPEG(gfx::NativeWindow window,
                              const gfx::Rect& source_rect,
                              GrabSnapshotDataCallback callback) {
  GrabWindowSnapshot(window, source_rect,
                     base::BindOnce(&EncodeImageAndScheduleCallback,
                                    &EncodeImageAsJPEG, std::move(callback)));
}

}  // namespace ui
