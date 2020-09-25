// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ACCESSIBILITY_INSPECT_AX_UTILS_H_
#define TOOLS_ACCESSIBILITY_INSPECT_AX_UTILS_H_

#include "content/public/browser/accessibility_tree_formatter.h"

namespace tools {

using TreeSelector = content::AccessibilityTreeFormatter::TreeSelector;

// Prints help for tree selectors like --pattern, --chromium etc.
void PrintHelpForTreeSelectors();

// Returns tree selector from command line arguments.
TreeSelector TreeSelectorFromCommandLine(const base::CommandLine*);

}  // namespace tools

#endif  // TOOLS_ACCESSIBILITY_INSPECT_AX_UTILS_H_
