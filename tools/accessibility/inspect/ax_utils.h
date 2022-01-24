// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ACCESSIBILITY_INSPECT_AX_UTILS_H_
#define TOOLS_ACCESSIBILITY_INSPECT_AX_UTILS_H_

#include "base/command_line.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace tools {

// Prints help for tree selectors like --pattern, --chromium etc.
void PrintHelpForTreeSelectors();

// Returns tree selector from command line arguments.
absl::optional<ui::AXTreeSelector> TreeSelectorFromCommandLine(
    const base::CommandLine& command_line);

}  // namespace tools

#endif  // TOOLS_ACCESSIBILITY_INSPECT_AX_UTILS_H_
