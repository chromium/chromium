// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_mask.h"

#include "cc/paint/paint_flags.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

namespace views {

InkDropMask::InkDropMask(const gfx::Size& layer_size)
    : layer_(ui::LAYER_TEXTURED) {
  layer_.set_delegate(this);
  layer_.SetBounds(gfx::Rect(layer_size));
  layer_.SetFillsBoundsOpaquely(false);
  layer_.SetName("InkDropMaskLayer");
}

InkDropMask::~InkDropMask() {
  layer_.set_delegate(nullptr);
}

void InkDropMask::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                             float new_device_scale_factor) {}

PathInkDropMask::PathInkDropMask(const gfx::Size& layer_size,
                                 const SkPath& path)
    : InkDropMask(layer_size), path_(path) {}

void PathInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setAlphaf(1.0f);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  ui::PaintRecorder recorder(context, layer()->size());
  recorder.canvas()->DrawPath(path_, flags);
}

}  // namespace views
