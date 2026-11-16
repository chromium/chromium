// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ERROR_TYPES_H_
#define UI_ACCESSIBILITY_AX_ERROR_TYPES_H_

#include "base/containers/enum_set.h"

namespace ui {

// Flags for the possible ways an AXTree serialization could fail.
enum class AXSerializationErrorFlag {
  kNoErrors = 0,
  kMinValue = kNoErrors,
  kMaxNodesReached,
  kTimeoutReached,
  kMaxValue = kTimeoutReached,
};

using AXSerializationErrorFlagSet = base::EnumSet<AXSerializationErrorFlag>;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ERROR_TYPES_H_
