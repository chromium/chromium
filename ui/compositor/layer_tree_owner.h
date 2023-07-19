// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_TREE_OWNER_H_
#define UI_COMPOSITOR_LAYER_TREE_OWNER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Layer;

// Scoping object that owns a Layer and all its descendants.
class COMPOSITOR_EXPORT LayerTreeOwner {
 public:
  explicit LayerTreeOwner(std::unique_ptr<Layer> root);

  LayerTreeOwner(const LayerTreeOwner&) = delete;
  LayerTreeOwner& operator=(const LayerTreeOwner&) = delete;

  ~LayerTreeOwner();

  [[nodiscard]] Layer* release() {
    Layer* root = root_;
    root_ = nullptr;
    return root;
  }

  Layer* root() { return root_; }
  const Layer* root() const { return root_; }

 private:
  raw_ptr<Layer, AcrossTasksDanglingUntriaged> root_;
};

}  // namespace

#endif  // UI_COMPOSITOR_LAYER_TREE_OWNER_H_
