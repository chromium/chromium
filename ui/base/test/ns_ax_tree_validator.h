// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_NS_AX_TREE_VALIDATOR_H_
#define UI_BASE_TEST_NS_AX_TREE_VALIDATOR_H_

#include "base/optional.h"

@protocol NSAccessibility;

namespace ui {

struct NSAXTreeProblemDetails {
  enum ProblemType {
    // |node_a| (the child node) is not a child of |node_b| (its parent).
    NSAX_NOT_CHILD_OF_PARENT,

    // |node_a| (the child node) is a child of |node_b|, but |node_a|'s parent
    // is |node_c| instead.
    NSAX_CHILD_PARENT_NOT_THIS,

    // |node_a| (a supplied root node) is non-nil but does not conform to
    // NSAccessibility.
    NSAX_NOT_NSACCESSIBILITY,

    // |node_a| (the child node)'s parent |node_b| is non-nil but does not
    // conform to NSAccessibility.
    NSAX_PARENT_NOT_NSACCESSIBILITY,
  };

  NSAXTreeProblemDetails(ProblemType type, id node_a, id node_b, id node_c);

  ProblemType type;
  // These aren't id<NSAccessibility> because some kinds of problem are caused
  // by them not conforming to NSAccessibility.
  id node_a;
  id node_b;
  id node_c;

  std::string ToString();
};

// Validates the accessibility tree rooted at |root|. If at least one problem is
// found, returns an |AXTreeProblemDetails| as described above; if not, returns
// base::nullopt.
base::Optional<NSAXTreeProblemDetails> ValidateNSAXTree(
    id<NSAccessibility> root,
    size_t* nodes_visited);

// Prints the accessibility tree rooted at |root|. This function is useful for
// debugging failures of ValidateNSAXTree tests.
void PrintNSAXTree(id<NSAccessibility> root);

}  // ui

#endif  // UI_BASE_TEST_NS_AX_TREE_VALIDATOR_H_
