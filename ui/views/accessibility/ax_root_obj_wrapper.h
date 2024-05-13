// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/display/display_observer.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

class VIEWS_EXPORT AXRootObjWrapper : public views::AXAuraObjWrapper,
                                      display::DisplayObserver {
 public:
  AXRootObjWrapper(views::AXAuraObjCache::Delegate* delegate,
                   views::AXAuraObjCache* cache);
  AXRootObjWrapper(const AXRootObjWrapper&) = delete;
  AXRootObjWrapper& operator=(const AXRootObjWrapper&) = delete;
  ~AXRootObjWrapper() override;

  // Convenience method to check for existence of a child.
  bool HasChild(views::AXAuraObjWrapper* child);

  // views::AXAuraObjWrapper overrides.
  views::AXAuraObjWrapper* GetParent() override;
  void GetChildren(
      std::vector<raw_ptr<views::AXAuraObjWrapper, VectorExperimental>>*
          out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  ui::AXNodeID GetUniqueId() const final;
  std::string ToString() const override;

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  display::ScopedOptionalDisplayObserver display_observer_{this};
  const ui::AXUniqueId unique_id_{ui::AXUniqueId::Create()};

  raw_ptr<views::AXAuraObjCache::Delegate> delegate_;
};

#endif  // UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
