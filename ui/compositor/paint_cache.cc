// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_cache.h"

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/compositor/paint_context.h"

namespace ui {

PaintCache::PaintCache() = default;
PaintCache::~PaintCache() = default;

bool PaintCache::UseCache(const PaintContext& context,
                          const gfx::Size& size_in_context) {
  if (!record_ || context.device_scale_factor() != device_scale_factor_) {
    return false;
  }
  DCHECK(context.list_);
  context.list_->StartPaint();
  context.list_->push<cc::DrawRecordOp>(*record_);
  gfx::Rect bounds_in_layer = context.ToLayerSpaceBounds(size_in_context);
  context.list_->EndPaintOfUnpaired(bounds_in_layer);
  return true;
}

void PaintCache::SetPaintRecord(cc::PaintRecord record,
                                float device_scale_factor) {
  record_ = std::move(record);
  device_scale_factor_ = device_scale_factor;
}

}  // namespace ui
