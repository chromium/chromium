// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/hardware_capabilities.h"

namespace ui {

HardwareCapabilities::HardwareCapabilities() = default;

HardwareCapabilities::HardwareCapabilities(const HardwareCapabilities& other) =
    default;

HardwareCapabilities& HardwareCapabilities::operator=(
    const HardwareCapabilities& other) = default;

HardwareCapabilities::~HardwareCapabilities() = default;

}  // namespace ui
