// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_

#include <stdint.h>

#include <vector>

#include "base/scoped_observer.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace views {
class AXAuraObjCache;

// Describes a |Window| for use with other AX classes.
class AXWindowObjWrapper : public AXAuraObjWrapper,
                           public aura::WindowObserver {
 public:
  // |aura_obj_cache| and |window| must outlive this object.
  AXWindowObjWrapper(AXAuraObjCache* aura_obj_cache, aura::Window* window);
  AXWindowObjWrapper(const AXWindowObjWrapper&) = delete;
  AXWindowObjWrapper& operator=(const AXWindowObjWrapper&) = delete;
  ~AXWindowObjWrapper() override;

  // AXAuraObjWrapper overrides.
  bool HandleAccessibleAction(const ui::AXActionData& action) override;
  bool IsIgnored() override;
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32_t GetUniqueId() const final;
  std::string ToString() const override;

  // WindowObserver overrides.
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowTransformed(aura::Window* window,
                           ui::PropertyChangeReason reason) override;
  void OnWindowTitleChanged(aura::Window* window) override;

 private:
  // Fires an accessibility event.
  void FireEvent(ax::mojom::Event event_type);

  aura::Window* const window_;

  const bool is_root_window_;

  const ui::AXUniqueId unique_id_;

  // Whether OnWindowDestroying has happened for |window_|. Used to suppress
  // further events from |window| after OnWindowDestroying. Otherwise, dangling
  // pointer could be left in |aura_obj_cache_|. See https://crbug.com/1091545
  bool window_destroying_ = false;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
