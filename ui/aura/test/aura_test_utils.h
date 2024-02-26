// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_UTILS_H_
#define UI_AURA_TEST_AURA_TEST_UTILS_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"

namespace gfx {
class Point;
}

namespace aura {
class WindowEventDispatcher;
class WindowTreeHost;

namespace test {

const gfx::Point& QueryLatestMousePositionRequestInHost(WindowTreeHost* host);
void SetHostDispatcher(WindowTreeHost* host,
                       std::unique_ptr<WindowEventDispatcher> dispatcher);
void DisableIME(WindowTreeHost* host);
void DisableNativeWindowOcclusionTracking(WindowTreeHost* host);
const base::flat_set<raw_ptr<WindowTreeHost, CtnExperimental>>&
GetThrottledHosts();
bool AcceleratedWidgetMadeVisible(WindowTreeHost* host);

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_UTILS_H_
