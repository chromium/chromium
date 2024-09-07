// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_OBSERVER_H_
#define UI_AURA_WINDOW_TREE_HOST_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

class SkRegion;

namespace aura {
class WindowTreeHost;

class AURA_EXPORT WindowTreeHostObserver {
 public:
  // Called when the host's client size has changed.
  virtual void OnHostResized(WindowTreeHost* host) {}

  // Called when the host is moved on screen.
  virtual void OnHostMovedInPixels(WindowTreeHost* host) {}

  // Called when the host is moved to a different workspace.
  virtual void OnHostWorkspaceChanged(WindowTreeHost* host) {}

  // Called when the native window system sends the host request to close.
  virtual void OnHostCloseRequested(WindowTreeHost* host) {}

  // Called when the occlusion status of the native window changes, iff
  // occlusion tracking is enabled for a descendant of the root.
  virtual void OnOcclusionStateChanged(WindowTreeHost* host,
                                       Window::OcclusionState new_state,
                                       const SkRegion& occluded_region) {}

  // Called before processing a bounds change. The bounds change may result in
  // one or both of OnHostResized() and OnHostMovedInPixels() being called.
  // This is not supported by all WindowTreeHosts.
  // OnHostWillProcessBoundsChange() is always followed by
  // OnHostDidProcessBoundsChange().
  virtual void OnHostWillProcessBoundsChange(WindowTreeHost* host) {}
  virtual void OnHostDidProcessBoundsChange(WindowTreeHost* host) {}

  virtual void OnCompositingFrameSinksToThrottleUpdated(
      const aura::WindowTreeHost* host,
      const base::flat_set<viz::FrameSinkId>& ids) {}

  virtual void OnSetPreferredRefreshRate(WindowTreeHost* host,
                                         float preferred_refresh_rate) {}

 protected:
  virtual ~WindowTreeHostObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_OBSERVER_H_
