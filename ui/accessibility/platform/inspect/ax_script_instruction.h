// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_SCRIPT_INSTRUCTION_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_SCRIPT_INSTRUCTION_H_

#include <string>

#include "ui/accessibility/ax_export.h"

namespace ui {

class AXPropertyNode;

// A single script instruction. See AXInspectScenario.
// Note: this is only for executing tests or for running a script via
// ax_dump_tree/ax_dump_events for inspecting from out of process.
class AX_EXPORT AXScriptInstruction final {
 public:
  explicit AXScriptInstruction(const std::string& instruction);

  bool IsEvent() const;
  bool IsScript() const;
  bool IsComment() const;
  bool IsPrintTree() const;

  AXPropertyNode AsScript() const;
  std::string AsEvent() const;
  std::string AsComment() const;

 private:
  size_t EventNameStartIndex() const;
  std::string instruction_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_SCRIPT_INSTRUCTION_H_
