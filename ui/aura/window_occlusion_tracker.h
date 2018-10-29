// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_OCCLUSION_TRACKER_H_
#define UI_AURA_WINDOW_OCCLUSION_TRACKER_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"

struct SkIRect;
class SkRegion;

namespace gfx {
class Transform;
}

namespace aura {

namespace test {
class WindowOcclusionTrackerTestApi;
}

class Env;

// Notifies tracked Windows when their occlusion state change.
//
// To start tracking the occlusion state of a Window, call
//   aura::Window::TrackOcclusionState()
//
// A Window is occluded if its bounds and transform are not animated and one of
// these conditions is true:
// - The Window is hidden (Window::IsVisible() is true).
// - The bounds of the Window are completely covered by opaque and axis-aligned
//   Windows whose bounds and transform are not animated.
// Note that an occluded window may be drawn on the screen by window switching
// features such as "Alt-Tab" or "Overview".
class AURA_EXPORT WindowOcclusionTracker : public ui::LayerAnimationObserver,
                                           public WindowObserver {
 public:
  // Prevents window occlusion state computations within its scope. If an event
  // that could cause window occlusion states to change occurs within the scope
  // of a ScopedPause, window occlusion state computations are delayed until all
  // ScopedPause objects have been destroyed.
  // TODO(crbug.com/867150): Pause the tracker in Window Service under mus.
  class AURA_EXPORT ScopedPause {
   public:
    explicit ScopedPause(Env* env);
    ~ScopedPause();

   private:
    Env* const env_;
    DISALLOW_COPY_AND_ASSIGN(ScopedPause);
  };

  // Start tracking the occlusion state of |window|.
  void Track(Window* window);

  // Set a callback to determine whether a window has content to draw in
  // addition to layer type check (window layer type != ui::LAYER_NOT_DRAWN).
  using WindowHasContentCallback = base::RepeatingCallback<bool(const Window*)>;
  void set_window_has_content_callback(WindowHasContentCallback callback) {
    window_has_content_callback_ = std::move(callback);
  }

 private:
  friend class test::WindowOcclusionTrackerTestApi;
  friend class Env;
  friend std::unique_ptr<WindowOcclusionTracker>::deleter_type;

  struct RootWindowState {
    // Number of Windows whose occlusion state is tracked under this root
    // Window.
    int num_tracked_windows = 0;

    // Whether the occlusion state of tracked Windows under this root is stale.
    bool dirty = false;
  };

  WindowOcclusionTracker();
  ~WindowOcclusionTracker() override;

  // Recomputes the occlusion state of tracked windows under roots marked as
  // dirty in |root_windows_| if there are no active ScopedPause instance.
  void MaybeComputeOcclusion();

  // Recomputes the occlusion state of |window| and its descendants.
  // |parent_transform_relative_to_root| is the transform of |window->parent()|
  // relative to the root window. |clipped_bounds| is an optional mask for the
  // bounds of |window| and its descendants. |occluded_region| is a region
  // covered by windows which are on top of |window|. Returns true if at least
  // one window in the hierarchy starting at |window| is NOT_OCCLUDED.
  bool RecomputeOcclusionImpl(
      Window* window,
      const gfx::Transform& parent_transform_relative_to_root,
      const SkIRect* clipped_bounds,
      SkRegion* occluded_region);

  // Returns true if |window| opaquely fills its bounds. |window| must be
  // visible.
  bool VisibleWindowIsOpaque(Window* window) const;

  // Returns true if |window| has content.
  bool WindowHasContent(Window* window) const;

  // Removes windows whose bounds and transform are not animated from
  // |animated_windows_|. Marks the root of those windows as dirty.
  void CleanupAnimatedWindows();

  // If the bounds or transform of |window| are animated and |window| is not in
  // |animated_windows_|, adds |window| to |animated_windows_| and returns true.
  bool MaybeObserveAnimatedWindow(Window* window);

  // Calls SetOccluded() with |is_occluded| as argument for |window| and its
  // descendants.
  void SetWindowAndDescendantsAreOccluded(Window* window, bool is_occluded);

  // Updates the occlusion state of |window| in |tracked_windows_|, based on
  // |is_occluded| and window->IsVisible(). No-op if |window| is not in
  // |tracked_windows_|.
  void SetOccluded(Window* window, bool is_occluded);

  // Returns true if |window| is in |tracked_windows_|.
  bool WindowIsTracked(Window* window) const;

  // Returns true if |window| is in |animated_windows_|.
  bool WindowIsAnimated(Window* window) const;

  // If the root of |window| is not dirty and |predicate| is true, marks the
  // root of |window| as dirty. Then, calls MaybeComputeOcclusion().
  // |predicate| is not evaluated if the root of |window| is already dirty when
  // this is called.
  template <typename Predicate>
  void MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(Window* window,
                                                       Predicate predicate);

  // Marks |root_window| as dirty.
  void MarkRootWindowAsDirty(RootWindowState* root_window_state);

  // Returns true if |window| or one of its parents is in |animated_windows_|.
  bool WindowOrParentIsAnimated(Window* window) const;

  // Returns true if |window| or one of its descendants is in
  // |tracked_windows_| and visible.
  bool WindowOrDescendantIsTrackedAndVisible(Window* window) const;

  // Returns true if |window| or one of its descendants is visible, opaquely
  // fills its bounds and is not in |animated_windows_|. If
  // |assume_parent_opaque| is true, the function assumes that the combined
  // opacity of window->parent() is 1.0f. If |assume_window_opaque|, the
  // function assumes that the opacity of |window| is 1.0f.
  bool WindowOrDescendantIsOpaque(Window* window,
                                  bool assume_parent_opaque = false,
                                  bool assume_window_opaque = false) const;

  // Returns true if changing the opacity or alpha state of |window| could
  // affect the occlusion state of a tracked window.
  bool WindowOpacityChangeMayAffectOcclusionStates(Window* window) const;

  // Returns true if changing the transform, bounds or stacking order of
  // |window| could affect the occlusion state of a tracked window.
  bool WindowMoveMayAffectOcclusionStates(Window* window) const;

  // Called when a tracked |window| is added to a root window.
  void TrackedWindowAddedToRoot(Window* window);

  // Called when a tracked |window| is removed from a root window.
  void TrackedWindowRemovedFromRoot(Window* window);

  // Removes |this| from the observer list of |window| and its descendants,
  // except if they are in |tracked_windows_| or |windows_being_destroyed_|.
  void RemoveObserverFromWindowAndDescendants(Window* window);

  // Add |this| to the observer list of |window| and its descendants.
  void AddObserverToWindowAndDescendants(Window* window);

  // Pauses/unpauses the occlusion state computation.
  void Pause();
  void Unpause();

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

  // WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowAdded(Window* window) override;
  void OnWillRemoveWindow(Window* window) override;
  void OnWindowVisibilityChanged(Window* window, bool visible) override;
  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowOpacitySet(Window* window,
                          ui::PropertyChangeReason reason) override;
  void OnWindowAlphaShapeSet(Window* window) override;
  void OnWindowTransformed(Window* window,
                           ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(Window* window) override;
  void OnWindowDestroyed(Window* window) override;
  void OnWindowAddedToRootWindow(Window* window) override;
  void OnWindowRemovingFromRootWindow(Window* window,
                                      Window* new_root) override;
  void OnWindowLayerRecreated(Window* window) override;

  // Windows whose occlusion state is tracked.
  base::flat_map<Window*, Window::OcclusionState> tracked_windows_;

  // Windows whose bounds or transform are animated.
  //
  // To reduce the overhead of the WindowOcclusionTracker, windows in this set
  // and their descendants are considered non-occluded and cannot occlude other
  // windows. A window is added to this set the first time that occlusion is
  // computed after it was animated. It is removed when the animation ends or is
  // aborted.
  base::flat_set<Window*> animated_windows_;

  // Root Windows of Windows in |tracked_windows_|.
  base::flat_map<Window*, RootWindowState> root_windows_;

  // Number of times that occlusion has been recomputed in this process. We keep
  // track of this for tests.
  int num_times_occlusion_recomputed_ = 0;

  // Number of times that the current call to MaybeComputeOcclusion() has
  // recomputed occlusion states. Always 0 when not in MaybeComputeOcclusion().
  int num_times_occlusion_recomputed_in_current_step_ = 0;

  // Counter of the current occlusion tracking pause.
  int num_pause_occlusion_tracking_ = 0;

  // Tracks the observed windows.
  ScopedObserver<Window, WindowObserver> window_observer_{this};

  // Callback to be invoked for additional window has content check.
  WindowHasContentCallback window_has_content_callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionTracker);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_OCCLUSION_TRACKER_H_
