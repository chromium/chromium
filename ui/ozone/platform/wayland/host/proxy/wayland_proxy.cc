// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy.h"

#include "base/check.h"
#include "ui/platform_window/common/platform_window_defaults.h"

namespace wl {

namespace {
static WaylandProxy* g_instance = nullptr;
}

// static
WaylandProxy* WaylandProxy::GetInstance() {
  CHECK(ui::UseTestConfigForPlatformWindows());
  return g_instance;
}

// static
void WaylandProxy::SetInstance(WaylandProxy* instance) {
  CHECK(ui::UseTestConfigForPlatformWindows());
  g_instance = instance;
}

}  // namespace wl
