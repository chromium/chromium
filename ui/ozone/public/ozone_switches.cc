// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_switches.h"

namespace switches {

// Specify ozone platform implementation to use.
const char kOzonePlatform[] = "ozone-platform";

// Specify location for image dumps.
const char kOzoneDumpFile[] = "ozone-dump-file";

// Try to enable wayland input method editor.
const char kEnableWaylandIme[] = "enable-wayland-ime";

// Disable wayland input method editor.
const char kDisableWaylandIme[] = "disable-wayland-ime";

// Use explicit grab when opening popup windows.
// See https://crbug.com/1220274
const char kUseWaylandExplicitGrab[] = "use-wayland-explicit-grab";

// Disable explicit DMA-fences
const char kDisableExplicitDmaFences[] = "disable-explicit-dma-fences";

// Disable running as system compositor.
const char kDisableRunningAsSystemCompositor[] =
    "disable-running-as-system-compositor";

// Disable buffer bandwidth compression
const char kDisableBufferBWCompression[] = "disable-buffer-bw-compression";

// Specifies ozone screen size.
const char kOzoneOverrideScreenSize[] = "ozone-override-screen-size";

}  // namespace switches
