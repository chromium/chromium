// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PAINT_CACHE_H_
#define UI_COMPOSITOR_PAINT_CACHE_H_

#include <optional>

#include "cc/paint/paint_record.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class PaintContext;
class PaintRecorder;

// A class that holds the output of a PaintRecorder to be reused when the
// object that created the PaintRecorder has not been changed/invalidated.
class COMPOSITOR_EXPORT PaintCache {
 public:
  PaintCache();

  PaintCache(const PaintCache&) = delete;
  PaintCache& operator=(const PaintCache&) = delete;

  ~PaintCache();

  // Returns true if the PaintCache was able to insert a previously-saved
  // painting output into the PaintContext. If it returns false, the caller
  // needs to do the work of painting, which can be stored into the PaintCache
  // to be used next time.
  bool UseCache(const PaintContext& context, const gfx::Size& size_in_context);

 private:
  // Only PaintRecorder can modify these.
  friend PaintRecorder;

  void SetPaintRecord(cc::PaintRecord record, float device_scale_factor);

  // Stored in an sk_sp because PaintOpBuffer requires this to append the cached
  // items into it.
  std::optional<cc::PaintRecord> record_;

  // This allows paint cache to be device scale factor aware. If a request for
  // a cache entry is made that does not match the stored cache entry's DSF,
  // then we can reject it instead of returning the incorrect cache entry.
  // See https://crbug.com/834114 for details.
  float device_scale_factor_ = 0.f;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_PAINT_CACHE_H_
