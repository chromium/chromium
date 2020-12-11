// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_TREE_OWNER_H_
#define UI_COMPOSITOR_LAYER_TREE_OWNER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Layer;

// Scoping object that owns a Layer and all its descendants.
class COMPOSITOR_EXPORT LayerTreeOwner {
 public:
  explicit LayerTreeOwner(std::unique_ptr<Layer> root);
  ~LayerTreeOwner();

  Layer* release() WARN_UNUSED_RESULT {
    Layer* root = root_;
    root_ = nullptr;
    return root;
  }

  Layer* root() { return root_; }
  const Layer* root() const { return root_; }

 private:
  Layer* root_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeOwner);
};

}  // namespace

#endif  // UI_COMPOSITOR_LAYER_TREE_OWNER_H_
