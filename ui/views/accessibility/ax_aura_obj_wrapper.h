// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_WRAPPER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"

namespace ui {

struct AXActionData;

}  // namespace ui

namespace views {

class AXAuraObjCache;

// An interface abstraction for Aura views that exposes the view-tree formed
// by the implementing view types.
class VIEWS_EXPORT AXAuraObjWrapper {
 public:
  explicit AXAuraObjWrapper(AXAuraObjCache* cache);
  virtual ~AXAuraObjWrapper();

  // Traversal and serialization.
  virtual AXAuraObjWrapper* GetParent() = 0;
  virtual void GetChildren(
      std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>*
          out_children) = 0;
  virtual void Serialize(ui::AXNodeData* out_node_data) = 0;
  virtual ui::AXNodeID GetUniqueId() const = 0;
  virtual std::string ToString() const = 0;

  // Actions.
  virtual bool HandleAccessibleAction(const ui::AXActionData& action);

  const AXAuraObjCache* cache() const { return aura_obj_cache_; }

 protected:
  std::optional<std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>>
      cached_children_;

  // The cache associated with this wrapper. Subclasses should initialize this
  // cache on construction.
  raw_ptr<AXAuraObjCache> aura_obj_cache_ = nullptr;

  friend class AXTreeSourceViews;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_WRAPPER_H_
