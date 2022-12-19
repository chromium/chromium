// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_recorder.h"

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/compositor/paint_cache.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace ui {

// This class records a reference to the context, the canvas returned
// by its recorder_, and the cache. Thus all 3 of these must remain
// valid for the lifetime of this object.
// If a |cache| is provided, this records into the |cache|'s PaintOpBuffer
// directly, then appends that to the |context|. If not, then this records
// to the |context|'s PaintOpBuffer.
PaintRecorder::PaintRecorder(const PaintContext& context,
                             const gfx::Size& recording_size,
                             float recording_scale_x,
                             float recording_scale_y,
                             PaintCache* cache)
    : context_(context),
      record_canvas_(recording_size),
      canvas_(&record_canvas_, context.device_scale_factor_),
      cache_(cache),
      recording_size_(recording_size) {
#if DCHECK_IS_ON()
  DCHECK(!context_->inside_paint_recorder_);
  context_->inside_paint_recorder_ = true;
#endif
  if (context_->is_pixel_canvas()) {
    canvas()->Save();
    canvas()->Scale(recording_scale_x, recording_scale_y);
  }
}

// TODO(malaykeshav): The scaling of recording size needs to be handled case
// by case and the decision to perform the scale should be moved to the caller.
PaintRecorder::PaintRecorder(const PaintContext& context,
                             const gfx::Size& recording_size)
    : PaintRecorder(
          context,
          gfx::ScaleToRoundedSize(
              recording_size,
              context.is_pixel_canvas() ? context.device_scale_factor_ : 1.f),
          context.device_scale_factor_,
          context.device_scale_factor_,
          nullptr) {}

PaintRecorder::~PaintRecorder() {
#if DCHECK_IS_ON()
  context_->inside_paint_recorder_ = false;
#endif
  if (context_->is_pixel_canvas())
    canvas()->Restore();

  cc::PaintRecord record = record_canvas_.ReleaseAsRecord();
  if (cache_) {
    cache_->SetPaintRecord(record, context_->device_scale_factor());
  }

  DCHECK(context_->list_);
  context_->list_->StartPaint();
  context_->list_->push<cc::DrawRecordOp>(std::move(record));
  gfx::Rect bounds_in_layer = context_->ToLayerSpaceBounds(recording_size_);
  context_->list_->EndPaintOfUnpaired(bounds_in_layer);
}

}  // namespace ui
