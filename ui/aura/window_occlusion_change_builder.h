// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_OCCLUSION_CHANGE_BUILDER_H_
#define UI_AURA_WINDOW_OCCLUSION_CHANGE_BUILDER_H_

#include <memory>

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

class SkRegion;

namespace aura {

// Interface for WindowOcclusionTracker to update window occlusion info.
class AURA_EXPORT WindowOcclusionChangeBuilder {
 public:
  virtual ~WindowOcclusionChangeBuilder() = default;

  // Add an occlusion info change for |window|.
  // Note that setting Window::OcclusionState::UNKNOWN to a window is not
  // allowed.
  virtual void Add(Window* window,
                   Window::OcclusionState occlusion_state,
                   SkRegion occluded_region) = 0;

  // Factory to create the default implementation that updates occlusion info
  // on aura::Window.
  static std::unique_ptr<WindowOcclusionChangeBuilder> Create();
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_OCCLUSION_CHANGE_BUILDER_H_
