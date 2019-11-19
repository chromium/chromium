// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace ui {

namespace {

scoped_refptr<base::RefCountedMemory> EncodeImageAsPNG(
    const gfx::Image& image) {
  if (image.IsEmpty())
    return nullptr;
  std::vector<uint8_t> result;
  DCHECK(!image.AsImageSkia().GetRepresentation(1.0f).is_null());
  gfx::PNGCodec::FastEncodeBGRASkBitmap(image.AsBitmap(), true, &result);
  return base::RefCountedBytes::TakeVector(&result);
}

scoped_refptr<base::RefCountedMemory> EncodeImageAsJPEG(
    const gfx::Image& image) {
  std::vector<uint8_t> result;
  DCHECK(!image.AsImageSkia().GetRepresentation(1.0f).is_null());
  gfx::JPEG1xEncodedDataFromImage(image, 100, &result);
  return base::RefCountedBytes::TakeVector(&result);
}

void EncodeImageAndScheduleCallback(
    scoped_refptr<base::RefCountedMemory> (*encode_func)(const gfx::Image&),
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory> data)>
        callback,
    gfx::Image image) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(encode_func, std::move(image)), std::move(callback));
}

}  // namespace

void GrabWindowSnapshotAsyncPNG(gfx::NativeWindow window,
                                const gfx::Rect& source_rect,
                                GrabWindowSnapshotAsyncPNGCallback callback) {
  GrabWindowSnapshotAsync(
      window, source_rect,
      base::BindOnce(&EncodeImageAndScheduleCallback, &EncodeImageAsPNG,
                     std::move(callback)));
}

void GrabWindowSnapshotAsyncJPEG(gfx::NativeWindow window,
                                 const gfx::Rect& source_rect,
                                 GrabWindowSnapshotAsyncJPEGCallback callback) {
  GrabWindowSnapshotAsync(
      window, source_rect,
      base::BindOnce(&EncodeImageAndScheduleCallback, &EncodeImageAsJPEG,
                     std::move(callback)));
}

}  // namespace ui
