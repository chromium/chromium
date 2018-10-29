// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/env_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

class VIEWS_EXPORT AXRootObjWrapper : public views::AXAuraObjWrapper,
                                      display::DisplayObserver,
                                      aura::EnvObserver {
 public:
  explicit AXRootObjWrapper(views::AXAuraObjCache::Delegate* delegate);
  ~AXRootObjWrapper() override;

  // Returns an AXAuraObjWrapper for an alert window with title set to |text|.
  views::AXAuraObjWrapper* GetAlertForText(const std::string& text);

  // Convenience method to check for existence of a child.
  bool HasChild(views::AXAuraObjWrapper* child);

  // views::AXAuraObjWrapper overrides.
  bool IsIgnored() override;
  views::AXAuraObjWrapper* GetParent() override;
  void GetChildren(
      std::vector<views::AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  const ui::AXUniqueId& GetUniqueId() const override;

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;
  void OnWillDestroyEnv() override;

  ui::AXUniqueId unique_id_;

  std::unique_ptr<aura::Window> alert_window_;

  views::AXAuraObjCache::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AXRootObjWrapper);
};

#endif  // UI_VIEWS_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
