// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class AXAuraObjCache;

// Describes a |View| for use with other AX classes.
class AXViewObjWrapper : public AXAuraObjWrapper, public ViewObserver {
 public:
  // |aura_obj_cache| must outlive this object.
  AXViewObjWrapper(AXAuraObjCache* aura_obj_cache, View* view);
  AXViewObjWrapper(const AXViewObjWrapper&) = delete;
  AXViewObjWrapper& operator=(const AXViewObjWrapper&) = delete;
  ~AXViewObjWrapper() override;

  View* view() { return view_; }

  // AXAuraObjWrapper overrides.
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>*
                       out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  ui::AXNodeID GetUniqueId() const final;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;
  std::string ToString() const override;

  // ViewObserver overrides.
  void OnViewIsDeleting(View* observed_view) override;

 private:
  // This is never null, as we destroy ourselves when the view is deleted.
  const raw_ptr<View> view_;

  base::ScopedObservation<View, ViewObserver> observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_
