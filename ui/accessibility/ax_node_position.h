// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_POSITION_H_
#define UI_ACCESSIBILITY_AX_NODE_POSITION_H_

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_position.h"

namespace ui {

// AXNodePosition includes implementations of AXPosition methods which require
// knowledge of the AXPosition AXNodeType (which is unknown by AXPosition).
class AX_EXPORT AXNodePosition : public AXPosition<AXNodePosition, AXNode> {
 public:
  // Creates either a text or a tree position, depending on the type of the node
  // provided.
  static AXPositionInstance CreatePosition(
      const AXNode& node,
      int child_index_or_text_offset,
      ax::mojom::TextAffinity affinity = ax::mojom::TextAffinity::kDownstream);

  AXNodePosition();
  ~AXNodePosition() override;
  AXNodePosition(const AXNodePosition& other);

  AXPositionInstance Clone() const override;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_POSITION_H_
