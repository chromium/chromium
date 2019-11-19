// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PAINT_CACHE_H_
#define UI_COMPOSITOR_PAINT_CACHE_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class PaintOpBuffer;
}

namespace ui {
class PaintContext;
class PaintRecorder;

// A class that holds the output of a PaintRecorder to be reused when the
// object that created the PaintRecorder has not been changed/invalidated.
class COMPOSITOR_EXPORT PaintCache {
 public:
  PaintCache();
  ~PaintCache();

  // Returns true if the PaintCache was able to insert a previously-saved
  // painting output into the PaintContext. If it returns false, the caller
  // needs to do the work of painting, which can be stored into the PaintCache
  // to be used next time.
  bool UseCache(const PaintContext& context, const gfx::Size& size_in_context);

 private:
  // Only PaintRecorder can modify these.
  friend PaintRecorder;

  void SetPaintOpBuffer(sk_sp<cc::PaintOpBuffer> paint_op_buffer,
                        float device_scale_factor);

  // Stored in an sk_sp because PaintOpBuffer requires this to append the cached
  // items into it.
  sk_sp<cc::PaintOpBuffer> paint_op_buffer_;

  // This allows paint cache to be device scale factor aware. If a request for
  // a cache entry is made that does not match the stored cache entry's DSF,
  // then we can reject it instead of returning the incorrect cache entry.
  // See https://crbug.com/834114 for details.
  float device_scale_factor_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(PaintCache);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_PAINT_CACHE_H_
