// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace ui {
class InputMethod;
}

namespace views {
class AXAuraObjCache;

// Describes a |Window| for use with other AX classes.
class AXWindowObjWrapper : public AXAuraObjWrapper,
                           public ui::InputMethodObserver,
                           public aura::WindowObserver {
 public:
  // |aura_obj_cache| and |window| must outlive this object.
  AXWindowObjWrapper(AXAuraObjCache* aura_obj_cache, aura::Window* window);
  AXWindowObjWrapper(const AXWindowObjWrapper&) = delete;
  AXWindowObjWrapper& operator=(const AXWindowObjWrapper&) = delete;
  ~AXWindowObjWrapper() override;

  // AXAuraObjWrapper overrides.
  bool HandleAccessibleAction(const ui::AXActionData& action) override;
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>*
                       out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  ui::AXNodeID GetUniqueId() const final;
  std::string ToString() const override;

  // InputMethodObserver overrides.
  void OnFocus() override {}
  void OnBlur() override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;

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

  gfx::Rect GetCaretBounds(const ui::TextInputClient* client);

  const raw_ptr<aura::Window> window_;

  const bool is_root_window_;

  const ui::AXUniqueId unique_id_{ui::AXUniqueId::Create()};

  // Whether OnWindowDestroying has happened for |window_|. Used to suppress
  // further events from |window| after OnWindowDestroying. Otherwise, dangling
  // pointer could be left in |aura_obj_cache_|. See https://crbug.com/1091545
  bool window_destroying_ = false;

  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};

  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      ime_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
