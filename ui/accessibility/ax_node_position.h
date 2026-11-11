// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_POSITION_H_
#define UI_ACCESSIBILITY_AX_NODE_POSITION_H_

#include <optional>

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_position.h"

namespace ui {

// Returns a human-readable representation of `AXPositionKind`.
AX_EXPORT std::string ToString(const AXPositionKind kind);

// AXNodePosition includes implementations of AXPosition methods which require
// knowledge of the AXPosition AXNodeType (which is unknown by AXPosition).
class AX_EXPORT AXNodePosition : public AXPosition<AXNodePosition, AXNode> {
 public:
  // Creates either a text or a tree position. If `offset_type` is not
  // specified, infers whether to create a text or tree position from `node`.
  static AXPositionInstance CreatePosition(
      const AXNode& node,
      int child_index_or_text_offset,
      ax::mojom::TextAffinity affinity = ax::mojom::TextAffinity::kDownstream,
      std::optional<ax::mojom::OffsetType> offset_type = std::nullopt);

  AXNodePosition();
  ~AXNodePosition() override;
  AXNodePosition(const AXNodePosition& other);

  AXPositionInstance Clone() const override;

 private:
  // Return true if the provided node should always use a text position, rather
  // than a tree position.
  static bool IsTextPositionAnchor(const AXNode& node);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_POSITION_H_
