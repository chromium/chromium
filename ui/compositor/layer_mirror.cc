// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_mirror.h"

#include "base/check_op.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_context.h"

namespace ui::internal {

LayerMirror::LayerMirror(Layer* source, Layer* dest)
    : source_(source), dest_(dest) {
  dest_->AddObserver(this);
  dest_->set_delegate(this);
}

LayerMirror::~LayerMirror() {
  dest_->RemoveObserver(this);
  dest_->set_delegate(nullptr);
}

void LayerMirror::OnPaintLayer(const PaintContext& context) {
  if (auto* delegate = source_->delegate()) {
    delegate->OnPaintLayer(context);
  }
}

void LayerMirror::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                             float new_device_scale_factor) {}

void LayerMirror::LayerDestroyed(Layer* layer) {
  DCHECK_EQ(dest_, layer);
  source_->OnMirrorDestroyed(this);
}

}  // namespace ui::internal
