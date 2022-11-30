// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_script_instruction.h"

#include <cstring>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

namespace ui {

constexpr char kPrintTree[] = "print tree";
constexpr char kWaitFor[] = "wait for ";
constexpr size_t kWaitForLength = sizeof(kWaitFor) / sizeof(kWaitFor[0]) - 1;
constexpr char kPress[] = "press ";
constexpr size_t kPressLength = sizeof(kPress) / sizeof(kPress[0]) - 1;

AXScriptInstruction::AXScriptInstruction(const std::string& instruction)
    : instruction_(instruction) {}

bool AXScriptInstruction::IsEvent() const {
  return !IsComment() && EventNameStartIndex() != std::string::npos;
}
bool AXScriptInstruction::IsKeyEvent() const {
  return base::StartsWith(instruction_, kPress);
}
bool AXScriptInstruction::IsScript() const {
  return !IsComment() && !IsEvent() && !IsKeyEvent() && !IsPrintTree();
}
bool AXScriptInstruction::IsComment() const {
  return base::StartsWith(instruction_, "//");
}
bool AXScriptInstruction::IsPrintTree() const {
  return instruction_ == kPrintTree;
}

AXPropertyNode AXScriptInstruction::AsScript() const {
  DCHECK(!IsEvent());
  return AXPropertyNode::From(instruction_);
}

std::string AXScriptInstruction::AsEvent() const {
  DCHECK(IsEvent());
  return instruction_.substr(kWaitForLength);
}

std::string AXScriptInstruction::AsDomKeyString() const {
  DCHECK(IsKeyEvent());
  return instruction_.substr(kPressLength);
}

std::string AXScriptInstruction::AsComment() const {
  DCHECK(IsComment());
  return instruction_;
}

size_t AXScriptInstruction::EventNameStartIndex() const {
  return instruction_.find(kWaitFor);
}

}  // namespace ui
