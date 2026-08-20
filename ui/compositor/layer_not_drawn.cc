// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_not_drawn.h"

#include "base/memory/scoped_refptr.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"

namespace ui {

LayerNotDrawn::LayerNotDrawn() : Layer(LAYER_NOT_DRAWN) {
  content_layer_ = cc::PictureLayer::Create(this);
  cc_layer_ = content_layer_.get();
  InitializeCcLayer();
  cc_layer_->SetIsDrawable(false);
}

LayerNotDrawn::~LayerNotDrawn() {
  Destroy();
}

bool LayerNotDrawn::ShouldSchedulePaint() const {
  // LayerNotDrawn does not draw any content, so it never needs to paint.
  return false;
}

scoped_refptr<cc::DisplayItemList> LayerNotDrawn::PaintContentsToDisplayList() {
  return base::MakeRefCounted<cc::DisplayItemList>();
}

bool LayerNotDrawn::FillsBoundsCompletely() const {
  return false;
}

void LayerNotDrawn::Reset() {
  content_layer_->ClearClient();
  content_layer_ = nullptr;
}

}  // namespace ui
