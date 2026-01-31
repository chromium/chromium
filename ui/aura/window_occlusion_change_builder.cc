// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_occlusion_change_builder.h"

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window_tracker.h"

namespace aura {

// Provide updating of occlusion info on aura::Window.
class DefaultWindowOcclusionChangeBuilder
    : public WindowOcclusionChangeBuilder {
 public:
  explicit DefaultWindowOcclusionChangeBuilder(bool disallow_unknown)
      : disallow_unknown_state_(disallow_unknown) {}

  DefaultWindowOcclusionChangeBuilder(
      const DefaultWindowOcclusionChangeBuilder&) = delete;
  DefaultWindowOcclusionChangeBuilder& operator=(
      const DefaultWindowOcclusionChangeBuilder&) = delete;

  ~DefaultWindowOcclusionChangeBuilder() override {
    // No frame eviction until all occlusion state changes are applied.
    viz::FrameEvictionManager::ScopedPause scoped_frame_eviction_pause;

    while (!windows_.windows().empty()) {
      Window* window = windows_.Pop();
      auto it = changes_.find(window);
      if (it == changes_.end())
        continue;
      window->SetOcclusionInfo(it->second.occlusion_state,
                               it->second.occluded_region);
    }
    changes_.clear();
  }

 private:
  struct OcclusionData {
    Window::OcclusionState occlusion_state;
    SkRegion occluded_region;
  };

  // WindowOcclusionChangeBuilder:
  void Add(Window* window,
           Window::OcclusionState occlusion_state,
           SkRegion occluded_region) override {
    if (disallow_unknown_state_) {
      // Change back to UNKNOWN is not allowed by default.
      // TODO(crbug.com/436906707): change this to CHECK.
      DCHECK_NE(occlusion_state, Window::OcclusionState::UNKNOWN);
    }

    windows_.Add(window);
    changes_[window] = {occlusion_state, occluded_region};
  }

  // Tracks live windows that has a change. This is needed in addition to the
  // keys in |changes_| because the window tree may change while changes are
  // accumulated or being applied.
  WindowTracker windows_;

  // Stores the accumulated occlusion changes.
  base::flat_map<Window*, OcclusionData> changes_;

  const bool disallow_unknown_state_;
};

// static
std::unique_ptr<WindowOcclusionChangeBuilder>
WindowOcclusionChangeBuilder::Create(bool disallow_unknown) {
  return std::make_unique<DefaultWindowOcclusionChangeBuilder>(
      disallow_unknown);
}

}  // namespace aura
