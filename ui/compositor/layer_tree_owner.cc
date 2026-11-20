// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_tree_owner.h"

#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer.h"

namespace ui {

namespace {

// Deletes |layer| and all its descendants.
void DeepDeleteLayers(Layer* layer) {
  std::vector<raw_ptr<Layer, DanglingUntriaged>> children = layer->children();
  for (Layer* child : children) {
    DeepDeleteLayers(child);
  }
  delete layer;
}

}  // namespace

LayerTreeOwner::LayerTreeOwner(std::unique_ptr<Layer> root)
    : root_(root.release()) {}

LayerTreeOwner::~LayerTreeOwner() {
  if (root_) {
    // Destroying an animating layer does not notify its implicit animation
    // observers by default, which leaks observers that delete themselves on
    // completion. Complete the animations first so they are notified.
    root_->CompleteAllAnimations();
    DeepDeleteLayers(root_);
  }
}

}  // namespace ui
