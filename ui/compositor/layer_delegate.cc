// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_delegate.h"

namespace ui {

void LayerDelegate::OnLayerBoundsChanged(const gfx::Rect& old_bounds,
                                         PropertyChangeReason reason) {}

void LayerDelegate::OnLayerTransformed(const gfx::Transform& old_transform,
                                       PropertyChangeReason reason) {}

void LayerDelegate::OnLayerOpacityChanged(PropertyChangeReason reason) {}

void LayerDelegate::OnLayerAlphaShapeChanged() {}

void LayerDelegate::OnLayerFillsBoundsOpaquelyChanged(
    PropertyChangeReason reason) {}

void LayerDelegate::OnLayerClipRectChanged(const gfx::Rect& old_rect,
                                           PropertyChangeReason reason) {}

void LayerDelegate::UpdateVisualState() {}

}  // namespace ui
