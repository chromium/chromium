// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_WRAPPER_H_

#include <vector>

#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/views_export.h"

namespace views {

class AXAuraObjWrapper;
class AXVirtualView;

// Wraps (and adapts) an AXVirtualView for use with AXTreeSourceViews.
class AXVirtualViewWrapper : public AXAuraObjWrapper {
 public:
  AXVirtualViewWrapper(AXVirtualView* virtual_view, AXAuraObjCache* cache);
  AXVirtualViewWrapper(const AXVirtualViewWrapper&) = delete;
  AXVirtualViewWrapper& operator=(const AXVirtualViewWrapper&) = delete;
  ~AXVirtualViewWrapper() override;

  // AXAuraObjWrapper:
  bool IsIgnored() override;
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32_t GetUniqueId() const override;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;

 private:
  // Weak.
  AXVirtualView* virtual_view_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_WRAPPER_H_
