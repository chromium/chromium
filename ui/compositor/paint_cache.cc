// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_cache.h"

#include <cmath>

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/compositor/paint_context.h"

namespace ui {

namespace {
constexpr float kSubpixelEpsilon = 0.001f;
}  // namespace

PaintCache::PaintCache() = default;
PaintCache::~PaintCache() = default;

bool PaintCache::UseCache(const PaintContext& context,
                          const gfx::Size& size_in_context) {
  if (!record_ || context.device_scale_factor() != device_scale_factor_) {
    return false;
  }
  if (!context.is_pixel_canvas()) {
    // The record has images snapped to whole device pixels for the offset it
    // was painted at. It can be reused at another offset only if the move is
    // a whole number of device pixels.
    const gfx::Vector2d delta = context.offset_ - offset_;
    const float dx = delta.x() * device_scale_factor_;
    const float dy = delta.y() * device_scale_factor_;
    if (std::abs(dx - std::round(dx)) > kSubpixelEpsilon ||
        std::abs(dy - std::round(dy)) > kSubpixelEpsilon) {
      return false;
    }
  }
  CHECK(context.list_, base::NotFatalUntil::M163);
  context.list_->StartPaint();
  context.list_->push<cc::DrawRecordOp>(*record_);
  gfx::Rect bounds_in_layer = context.ToLayerSpaceBounds(size_in_context);
  context.list_->EndPaintOfUnpaired(bounds_in_layer);
  return true;
}

void PaintCache::SetPaintRecord(cc::PaintRecord record,
                                float device_scale_factor,
                                const gfx::Vector2d& offset) {
  record_ = std::move(record);
  device_scale_factor_ = device_scale_factor;
  offset_ = offset;
}

}  // namespace ui
