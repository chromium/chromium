// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_position.h"

#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/buildflags.h"

namespace ui {

// On some platforms, most objects are represented in the text of their parents
// with a special "embedded object character" and not with their actual text
// contents. Also on the same platforms, if a node has only ignored descendants,
// i.e., it appears to be empty to assistive software, we need to treat it as a
// character and a word boundary.
AXEmbeddedObjectBehavior g_ax_embedded_object_behavior =
#if defined(OS_WIN) || BUILDFLAG(USE_ATK)
    AXEmbeddedObjectBehavior::kExposeCharacter;
#else
    AXEmbeddedObjectBehavior::kSuppressCharacter;
#endif  // defined(OS_WIN) || BUILDFLAG(USE_ATK)

// static
AXNodePosition::AXPositionInstance AXNodePosition::CreatePosition(
    const AXNode& node,
    int child_index_or_text_offset,
    ax::mojom::TextAffinity affinity) {
  if (!node.tree())
    return CreateNullPosition();

  AXTreeID tree_id = node.tree()->GetAXTreeID();
  if (node.IsLeaf()) {
    return CreateTextPosition(tree_id, node.id(), child_index_or_text_offset,
                              affinity);
  }

  return CreateTreePosition(tree_id, node.id(), child_index_or_text_offset);
}

AXNodePosition::AXNodePosition() = default;

AXNodePosition::~AXNodePosition() = default;

AXNodePosition::AXNodePosition(const AXNodePosition& other)
    : AXPosition<AXNodePosition, AXNode>(other) {}

AXNodePosition::AXPositionInstance AXNodePosition::Clone() const {
  return AXPositionInstance(new AXNodePosition(*this));
}

}  // namespace ui
