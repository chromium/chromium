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
  std::vector<raw_ptr<Layer, VectorExperimental>> children = layer->children();
  for (std::vector<raw_ptr<Layer, VectorExperimental>>::const_iterator it =
           children.begin();
       it != children.end(); ++it) {
    Layer* child = *it;
    DeepDeleteLayers(child);
  }
  delete layer;
}

}  // namespace

LayerTreeOwner::LayerTreeOwner(std::unique_ptr<Layer> root)
    : root_(root.release()) {}

LayerTreeOwner::~LayerTreeOwner() {
  if (root_)
    DeepDeleteLayers(root_);
}

}  // namespace ui
