// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_ID_FORWARD_H_
#define UI_ACCESSIBILITY_AX_NODE_ID_FORWARD_H_

#include <stdint.h>

namespace ui {

// Defines the type used for AXNode IDs.
using AXNodeID = int32_t;

// If a node is not yet or no longer valid, its ID should have a value of
// kInvalidAXNodeID.
static constexpr AXNodeID kInvalidAXNodeID = 0;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_ID_FORWARD_H_
