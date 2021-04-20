// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EXTRA_WINDOW_OCCLUSION_WIN_H_
#define UI_AURA_EXTRA_WINDOW_OCCLUSION_WIN_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura_extra/aura_extra_export.h"

namespace aura_extra {

// Computes the native window occlusion status for each aura::WindowTreeHost in
// |windows|.
AURA_EXTRA_EXPORT
base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
ComputeNativeWindowOcclusionStatus(
    const std::vector<aura::WindowTreeHost*>& windows);

}  // namespace aura_extra

#endif  // UI_AURA_EXTRA_WINDOW_OCCLUSION_WIN_H_
