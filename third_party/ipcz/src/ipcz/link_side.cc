// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/link_side.h"

namespace ipcz {

// static
constexpr LinkSide::Value LinkSide::kA;

// static
constexpr LinkSide::Value LinkSide::kB;

std::string LinkSide::ToString() const {
  return value_ == Value::kA ? "A" : "B";
}

}  // namespace ipcz
