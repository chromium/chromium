// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_HYPERTEXT_H_
#define UI_ACCESSIBILITY_AX_HYPERTEXT_H_

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"

namespace ui {

// Stores the hypertext for an `AXNode` in the accessibility tree. Hypertext has
// nothing to do with HTML but is how displayed text and embedded objects are
// represented in ATK and IAccessible2 APIs.
//
// Hypertext is computed as follows: If this node is a leaf, returns the inner
// text of this node. This is equivalent to its visible accessible name.
// Otherwise, if this node is not a leaf, represents every non-textual child
// node with a special "embedded object character", and every textual child node
// with its inner text. Textual nodes include e.g. static text and white space.
// Each non-textual child node is also called a hyperlink.
struct AX_EXPORT AXHypertext {
  AXHypertext();
  ~AXHypertext();
  AXHypertext(const AXHypertext& other);
  AXHypertext& operator=(const AXHypertext& other);

  // A flag that should be set if the hypertext information in this struct is
  // out-of-date and needs to be updated. This flag should always be set upon
  // construction because constructing this struct doesn't compute the
  // hypertext.
  bool needs_update = true;

  // Maps an embedded character offset in |hypertext| to an index in the list of
  // unignored children. A hyperlink is defined as any non-textual child.
  std::map<int, int> hypertext_offset_to_hyperlink_child_index;

  // See class comment for information on how this is computed.
  std::u16string hypertext;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_HYPERTEXT_H_
