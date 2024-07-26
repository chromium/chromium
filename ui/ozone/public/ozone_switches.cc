// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_switches.h"

namespace switches {

// Specify ozone platform implementation to use.
const char kOzonePlatform[] = "ozone-platform";

// Suggests the ozone platform to use (desktop Linux only).  Can be set on
// chrome://flags.  See https://crbug.com/1246928.
const char kOzonePlatformHint[] = "ozone-platform-hint";

// Specify location for image dumps.
const char kOzoneDumpFile[] = "ozone-dump-file";

// Try to enable wayland input method editor.
const char kEnableWaylandIme[] = "enable-wayland-ime";

// Disable wayland input method editor.
const char kDisableWaylandIme[] = "disable-wayland-ime";

// Specify wayland text-input protocol version.
// Defaults to "1" for text-input-v1. Can specify value "3" for experimental
// text-input-v3 support.
const char kWaylandTextInputVersion[] = "wayland-text-input-version";

// Use explicit grab when opening popup windows.
// See https://crbug.com/1220274
const char kUseWaylandExplicitGrab[] = "use-wayland-explicit-grab";

// Disable explicit DMA-fences
const char kDisableExplicitDmaFences[] = "disable-explicit-dma-fences";

// Specifies ozone screen size.
const char kOzoneOverrideScreenSize[] = "ozone-override-screen-size";

}  // namespace switches
