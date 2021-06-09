// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_COMPUTED_NODE_DATA_H_
#define UI_ACCESSIBILITY_AX_COMPUTED_NODE_DATA_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

class AXNode;

// Computes and stores information about an `AXNode` that is slow or error-prone
// to compute in the tree's source, e.g. in Blink. This class holds cached
// values that should be re-computed when the associated `AXNode` is in any way
// modified.
class AX_EXPORT AXComputedNodeData final {
 public:
  explicit AXComputedNodeData(const AXNode& node);
  virtual ~AXComputedNodeData();
  AXComputedNodeData(const AXComputedNodeData& other) = delete;
  AXComputedNodeData& operator=(const AXComputedNodeData& other) = delete;

  // Retrieves from the cache or computes the on-screen text that is found
  // inside the associated node and all its descendants, caches the result, and
  // returns a reference to the cached text.
  const std::string& GetOrComputeInnerTextUTF8() const;
  const std::u16string& GetOrComputeInnerTextUTF16() const;

  // Returns the length of the on-screen text that is found inside the
  // associated node and all its descendants. The text is either retrieved from
  // the cache, or computed and then cached.
  int GetOrComputeInnerTextLengthUTF8() const;
  int GetOrComputeInnerTextLengthUTF16() const;

 private:
  // Computes the on-screen text that is found inside the associated node and
  // all its descendants.
  std::string ComputeInnerTextUTF8() const;
  std::u16string ComputeInnerTextUTF16() const;

  // The node that is associated with this instance. Weak, owns us.
  const AXNode* const owner_;

  // Stores the on-screen text that is found inside the associated node and all
  // its descendants.
  //
  // Only one copy (either UTF8 or UTF16) should be cached as each platform
  // should only need one of the encodings.
  mutable absl::optional<std::string> inner_text_utf8_;
  mutable absl::optional<std::u16string> inner_text_utf16_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_COMPUTED_NODE_DATA_H_
