// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/link_type.h"

namespace ipcz {

// static
constexpr LinkType::Value LinkType::kCentral;

// static
constexpr LinkType::Value LinkType::kPeripheralInward;

// static
constexpr LinkType::Value LinkType::kPeripheralOutward;

// static
constexpr LinkType::Value LinkType::kBridge;

std::string LinkType::ToString() const {
  switch (value_) {
    case Value::kCentral:
      return "central";
    case Value::kPeripheralInward:
      return "peripheral-inward";
    case Value::kPeripheralOutward:
      return "peripheral-outward";
    case Value::kBridge:
      return "bridge";
  }
}

}  // namespace ipcz
