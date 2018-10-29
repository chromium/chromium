// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/ax_tree_source_mus.h"

#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/mus/ax_remote_host.h"

namespace views {

AXTreeSourceMus::AXTreeSourceMus(AXAuraObjWrapper* root,
                                 const ui::AXTreeID& tree_id)
    : root_(root), tree_id_(tree_id) {
  DCHECK(root_);
  DCHECK_NE(tree_id_, ui::AXTreeIDUnknown());
}

AXTreeSourceMus::~AXTreeSourceMus() = default;

bool AXTreeSourceMus::GetTreeData(ui::AXTreeData* tree_data) const {
  tree_data->tree_id = tree_id_;
  return AXTreeSourceViews::GetTreeData(tree_data);
}

AXAuraObjWrapper* AXTreeSourceMus::GetRoot() const {
  return root_;
}

void AXTreeSourceMus::SerializeNode(AXAuraObjWrapper* node,
                                    ui::AXNodeData* out_data) const {
  if (IsEqual(node, root_)) {
    node->Serialize(out_data);
    // Root is a contents view with an offset from the containing Widget.
    // However, the contents view in the host (browser) already has an offset
    // from its Widget, so the root should start at (0,0).
    out_data->location.set_origin(gfx::PointF());

    // Adjust for display device scale factor.
    if (device_scale_factor_ == 1.f) {
      // The AX system represents the identity transform with null.
      out_data->transform.reset();
    } else {
      out_data->transform = std::make_unique<gfx::Transform>();
      out_data->transform->Scale(device_scale_factor_, device_scale_factor_);
    }
    return;
  }

  AXTreeSourceViews::SerializeNode(node, out_data);
}

}  // namespace views
