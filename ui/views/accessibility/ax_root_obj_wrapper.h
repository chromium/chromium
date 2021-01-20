// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

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
  bool IsIgnored() override;
  views::AXAuraObjWrapper* GetParent() override;
  void GetChildren(
      std::vector<views::AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32_t GetUniqueId() const override;
  std::string ToString() const override;

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  ui::AXUniqueId unique_id_;

  views::AXAuraObjCache::Delegate* delegate_;
};

#endif  // UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
