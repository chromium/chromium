// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/virtual_display_util.h"

#if BUILDFLAG(IS_MAC)
#include "ui/display/mac/test/virtual_display_mac_util.h"
#endif

namespace display::test {

#if !BUILDFLAG(IS_MAC)

// Stub definitions for unimplemented platforms to prevent linker errors.

struct DisplayParams {};
const DisplayParams VirtualDisplayUtil::k1920x1080 = DisplayParams{};
const DisplayParams VirtualDisplayUtil::k1024x768 = DisplayParams{};

// static
std::unique_ptr<VirtualDisplayUtil> VirtualDisplayUtil::TryCreate(
    Screen* screen) {
  return nullptr;
}

#endif

}  // namespace display::test
