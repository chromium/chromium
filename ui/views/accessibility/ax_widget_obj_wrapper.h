// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class AXAuraObjCache;

// Describes a |Widget| for use with other AX classes.
class AXWidgetObjWrapper : public AXAuraObjWrapper, public WidgetObserver {
 public:
  // |aura_obj_cache| must outlive this object.
  AXWidgetObjWrapper(AXAuraObjCache* aura_obj_cache, Widget* widget);
  AXWidgetObjWrapper(const AXWidgetObjWrapper&) = delete;
  AXWidgetObjWrapper& operator=(const AXWidgetObjWrapper&) = delete;
  ~AXWidgetObjWrapper() override;

  // AXAuraObjWrapper overrides.
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>*
                       out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  ui::AXNodeID GetUniqueId() const final;
  std::string ToString() const override;

  // WidgetObserver overrides.
  void OnWidgetDestroying(Widget* widget) override;
  void OnWidgetDestroyed(Widget* widget) override;

 private:
  raw_ptr<Widget> widget_;

  const ui::AXUniqueId unique_id_{ui::AXUniqueId::Create()};

  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_
