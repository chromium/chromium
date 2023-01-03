// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_GPU_INFO_UTIL_H_
#define UI_DISPLAY_UTIL_GPU_INFO_UTIL_H_

#include "base/strings/string_piece_forward.h"
#include "base/values.h"

namespace display {

// Helpers for internal pages like chrome://gpu.  Create a dictionary with two
// values named description (string) and value (depends on the helper).
base::Value BuildGpuInfoEntry(base::StringPiece description,
                              base::StringPiece value);
base::Value::Dict BuildGpuInfoEntry(base::StringPiece description,
                                    base::Value value);

}  // namespace display

#endif  // UI_DISPLAY_UTIL_GPU_INFO_UTIL_H_
