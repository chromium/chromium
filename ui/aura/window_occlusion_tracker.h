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
#include "ui/aura/window_tree_host_observer.h"
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

class WindowOcclusionChangeBuilder;

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
                                           public WindowObserver,
                                           public WindowTreeHostObserver {
 public:
  // Prevents window occlusion state computations within its scope. If an event
  // that could cause window occlusion states to change occurs within the scope
  // of a ScopedPause, window occlusion state computations are delayed until all
  // ScopedPause objects have been destroyed.
  class AURA_EXPORT ScopedPause {
   public:
    ScopedPause();
    ~ScopedPause();

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedPause);
  };

  // Used to exclude a window and all descendants from occlusion calculation.
  // The occlusion state of the window and all descendants is set from the
  // the drawn state of the window, *not* based on what windows may be stacked
  // above them. Further, ignores the window that is excluded and its
  // descendants when computing the occlusion state of other windows in the
  // tree.
  //
  // This is useful for a window being dragged or resized to avoid unnecessary
  // occlusion state change triggered by these operation, because the window
  // bounds are temporary until it is finished.
  //
  // Note that this is intended to be used by window manager, such as Ash.
  class AURA_EXPORT ScopedExclude : public WindowObserver {
   public:
    explicit ScopedExclude(Window* window);
    ~ScopedExclude() override;

    Window* window() { return window_; }

   private:
    // WindowObserver:
    void OnWindowDestroying(Window* window) override;

    void Shutdown();

    Window* window_;
    DISALLOW_COPY_AND_ASSIGN(ScopedExclude);
  };

  // Forces the occlusion state of a window to VISIBLE regardless of the drawn
  // state of the window. Causes the occlusion state of descendants of the
  // window that is forced VISIBLE to be computed as if they were in an
  // isolated tree with a root that is drawn. Ignores the window that is forced
  // VISIBLE and its descendants when computing the occlusion state of other
  // windows in the tree.
  //
  // This function is primarily useful for situations that show the contents of
  // a hidden window, such as overview mode on ChromeOS.
  class AURA_EXPORT ScopedForceVisible : public WindowObserver {
   public:
    explicit ScopedForceVisible(Window* window);
    ~ScopedForceVisible() override;

   private:
    // WindowObserver:
    void OnWindowDestroying(Window* window) override;

    void Shutdown();

    Window* window_;
    DISALLOW_COPY_AND_ASSIGN(ScopedForceVisible);
  };

  // Holds occlusion related information for tracked windows.
  struct OcclusionData {
    // Occlusion state for a tracked window.
    Window::OcclusionState occlusion_state = Window::OcclusionState::UNKNOWN;
    // Region in root window coordinates that is occluded.
    SkRegion occluded_region;
  };

  // Start tracking the occlusion state of |window|.
  void Track(Window* window);

  // Compute the occlusion state and occluded region that |window| will have
  // once all bounds, transform, opacity, and visibility animations have
  // completed. |window| must be a window that has its occlusion state tracked.
  OcclusionData ComputeTargetOcclusionForWindow(Window* window);

  // Returns true if there are ignored animating windows.
  bool HasIgnoredAnimatingWindows() const { return !animated_windows_.empty(); }

  // Set a callback to determine whether a window has content to draw in
  // addition to layer type check (window layer type != ui::LAYER_NOT_DRAWN).
  using WindowHasContentCallback = base::RepeatingCallback<bool(const Window*)>;
  void set_window_has_content_callback(WindowHasContentCallback callback) {
    window_has_content_callback_ = std::move(callback);
  }

  // Set the factory to create WindowOcclusionChangeBuilder.
  using OcclusionChangeBuilderFactory =
      base::RepeatingCallback<std::unique_ptr<WindowOcclusionChangeBuilder>()>;
  void set_occlusion_change_builder_factory(
      OcclusionChangeBuilderFactory factory) {
    occlusion_change_builder_factory_ = std::move(factory);
  }

  bool IsPaused() const { return num_pause_occlusion_tracking_; }

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

    // The occlusion state of the root window's host.
    Window::OcclusionState occlusion_state = Window::OcclusionState::UNKNOWN;
  };

  WindowOcclusionTracker();
  ~WindowOcclusionTracker() override;

  // Returns true iff the occlusion states in |tracked_windows| match those
  // returned by Window::occlusion_state().
  static bool OcclusionStatesMatch(
      const base::flat_map<Window*, OcclusionData>& tracked_windows);

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

  // Returns true if |window| can occlude other windows (e.g. because it is
  // not transparent or has opaque regions for occlusion).
  // |window| must be visible.
  bool VisibleWindowCanOccludeOtherWindows(Window* window) const;

  // Returns true if |window| has content.
  bool WindowHasContent(Window* window) const;

  // Removes windows whose bounds and transform are not animated from
  // |animated_windows_|. Marks the root of those windows as dirty.
  void CleanupAnimatedWindows();

  // If the bounds or transform of |window| are animated and |window| is not in
  // |animated_windows_|, adds |window| to |animated_windows_| and returns true.
  bool MaybeObserveAnimatedWindow(Window* window);

  // Calls SetOccluded() with |is_occluded| as argument for |window| and its
  // descendants. |is_parent_visible| is true if the parent is visible.
  void SetWindowAndDescendantsAreOccluded(Window* window,
                                          bool is_occluded,
                                          bool is_parent_visible);

  // Updates the occlusion state of |window| in |tracked_windows_|, based on
  // |is_occluded| and window->IsVisible(). Updates the occluded region of
  // |window| using |occluded_region|. No-op if |window| is not in
  // |tracked_windows_|.
  void SetOccluded(Window* window,
                   bool is_occluded,
                   bool is_parent_visible,
                   const SkRegion& occluded_region);

  // Returns true if |window| is in |tracked_windows_|.
  bool WindowIsTracked(Window* window) const;

  // Returns true if |window| is in |animated_windows_|.
  bool WindowIsAnimated(Window* window) const;

  // Returns true if |window| is in |excluded_windows_|.
  bool WindowIsExcluded(Window* window) const;

  // Returns true if |window| is considered visible. Use this over IsVisible()
  // to ensure forced visible windows are considered.
  bool WindowIsVisible(Window* window) const;

  // Returns true if |window| is forced visible. This does *not* recurse and
  // only checks |window|. Use WindowIsVisible() to consider parents.
  bool WindowIsForcedVisible(Window* window) const;

  // If the root of |window| is not dirty and |predicate| is true, marks the
  // root of |window| as dirty. Then, calls MaybeComputeOcclusion().
  // |predicate| is not evaluated if the root of |window| is already dirty when
  // this is called.
  template <typename Predicate>
  void MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(Window* window,
                                                       Predicate predicate);

  // Marks |root_window_state| as dirty.
  void MarkRootWindowStateAsDirty(RootWindowState* root_window_state);

  // Marks |root_window| as dirty. Returns false if none of the descendent
  // windows in |root_window| are tracked.
  bool MarkRootWindowAsDirty(Window* root_window);

  // Returns true if |window| or one of its parents is in |animated_windows_|.
  bool WindowOrParentIsAnimated(Window* window) const;

  // Returns true if |window| or one of its descendants is in
  // |tracked_windows_| and visible.
  bool WindowOrDescendantIsTrackedAndVisible(Window* window) const;

  // Returns true if |window| or one of its descendants is visible, has some
  // opaque region and is not in |animated_windows_|. If |assume_parent_opaque|
  // is true, the function assumes that the combined opacity of window->parent()
  // is 1.0f. If |assume_window_opaque|, the function assumes that the opacity
  // of |window| is 1.0f.
  bool WindowOrDescendantCanOccludeOtherWindows(
      Window* window,
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

  // Exclude/Unexclude a window from occlusion tracking. See comment on
  // ScopedExclude.
  void Exclude(Window* window);
  void Unexclude(Window* window);

  // Called from ScopedForceVisible.
  void ForceWindowVisible(Window* window);
  void RemoveForceWindowVisible(Window* window);

  // Returns true if the occlusion tracker should use target bounds, opacity
  // transform, and visibility for occlusion computation. This will be true
  // if the target occlusion state of a window is being computed via
  // |ComputeTargetOcclusionForWindow|.
  bool ShouldUseTargetValues() const;

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
  void OnWindowTransparentChanged(Window* window) override;
  void OnWindowTransformed(Window* window,
                           ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(Window* window) override;
  void OnWindowDestroyed(Window* window) override;
  void OnWindowAddedToRootWindow(Window* window) override;
  void OnWindowRemovingFromRootWindow(Window* window,
                                      Window* new_root) override;
  void OnWindowLayerRecreated(Window* window) override;
  void OnWindowOpaqueRegionsForOcclusionChanged(Window* window) override;

  // WindowTreeHostObserver
  void OnOcclusionStateChanged(WindowTreeHost* host,
                               Window::OcclusionState new_state) override;

  // Windows whose occlusion data is tracked.
  base::flat_map<Window*, OcclusionData> tracked_windows_;

  // Windows whose target visibility is forced to true.
  base::flat_map<Window*, size_t> forced_visible_count_map_;

  // Windows whose bounds or transform are animated.
  //
  // To reduce the overhead of the WindowOcclusionTracker, windows in this set
  // and their descendants are considered non-occluded and cannot occlude other
  // windows. A window is added to this set the first time that occlusion is
  // computed after it was animated. It is removed when the animation ends or is
  // aborted.
  base::flat_set<Window*> animated_windows_;

  // Windows that are excluded from occlustion tracking. See comment on
  // ScopedExclude.
  base::flat_set<Window*> excluded_windows_;

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

  // Optional factory to create occlusion change builder.
  OcclusionChangeBuilderFactory occlusion_change_builder_factory_;

  // Stores the window for which the occlusion tracker is computing the
  // occlusion based on target bounds, opacity, transform, and visibility
  // values. If the occlusion tracker is not computing for a specific window
  // (most of the time it is not), this will be nullptr.
  Window* target_occlusion_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionTracker);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_OCCLUSION_TRACKER_H_
