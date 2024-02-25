// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_CHECKS_H_
#define UI_ACCESSIBILITY_AX_TREE_CHECKS_H_

#include <stddef.h>

#include "ui/accessibility/ax_base_export.h"

namespace ui {

// Data for AXTree checks that run when DHECKs are on.
// TODO(accessibility) Use the mojo struct directly in C++ code to avoid having
// to typemap it.
struct AX_BASE_EXPORT AXTreeChecks {
  AXTreeChecks() = default;
  virtual ~AXTreeChecks() = default;

  // If non-zero, provides the expected number of nodes in the tree after the
  // update is processed. This can be used to help check for mismatched trees.
  // Because a compromised renderer can lie about the number of nodes in the
  // tree, this should only be used for debugging, not validation that's trusted
  // on the browser side. Therefore, this should not be used with a CHECK, where
  // a compromised renderer could bring down the browser.
  // TODO(accessibility) To catch more errors than a DCHECK would, another
  // option is to use DumpWithoutCrashing when the counts don't match. This
  // could also be paired with mojo::ReportBadMessage to kill the renderer (or
  // perhaps resetting accessibility would be preferable).
  size_t node_count = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_CHECKS_H_
