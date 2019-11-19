// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_

#include <stdint.h>

#include "base/scoped_observer.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace views {
class AXAuraObjCache;

// Describes a |Widget| for use with other AX classes.
class AXWidgetObjWrapper : public AXAuraObjWrapper,
                           public WidgetObserver,
                           public WidgetRemovalsObserver {
 public:
  // |aura_obj_cache| must outlive this object.
  AXWidgetObjWrapper(AXAuraObjCache* aura_obj_cache, Widget* widget);
  AXWidgetObjWrapper(const AXWidgetObjWrapper&) = delete;
  AXWidgetObjWrapper& operator=(const AXWidgetObjWrapper&) = delete;
  ~AXWidgetObjWrapper() override;

  // AXAuraObjWrapper overrides.
  bool IsIgnored() override;
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32_t GetUniqueId() const final;

  // WidgetObserver overrides.
  void OnWidgetDestroying(Widget* widget) override;
  void OnWidgetClosing(Widget* widget) override;
  void OnWidgetVisibilityChanged(Widget*, bool) override;

  // WidgetRemovalsObserver overrides.
  void OnWillRemoveView(Widget* widget, View* view) override;

 private:
  Widget* widget_;

  const ui::AXUniqueId unique_id_;

  ScopedObserver<Widget, WidgetObserver> widget_observer_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_
