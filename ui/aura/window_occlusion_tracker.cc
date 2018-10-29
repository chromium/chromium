// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_occlusion_tracker.h"

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tracker.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"

namespace aura {

namespace {

// When one of these properties is animated, a window is considered non-occluded
// and cannot occlude other windows.
constexpr ui::LayerAnimationElement::AnimatableProperties
    kSkipWindowWhenPropertiesAnimated =
        ui::LayerAnimationElement::TRANSFORM |
        ui::LayerAnimationElement::BOUNDS | ui::LayerAnimationElement::OPACITY;

// Maximum number of times that MaybeComputeOcclusion() should have to recompute
// occlusion states before they become stable.
//
// TODO(fdoray): This can be changed to 2 once showing/hiding a WebContents
// doesn't cause a call to Show()/Hide() on the aura::Window of a
// RenderWidgetHostViewAura. https://crbug.com/827268
constexpr int kMaxRecomputeOcclusion = 3;

bool WindowOrParentHasShape(Window* window) {
  if (window->layer()->alpha_shape())
    return true;
  if (window->parent())
    return WindowOrParentHasShape(window->parent());
  return false;
}

// Returns the transform of |window| relative to its root.
// |parent_transform_relative_to_root| is the transform of |window->parent()|
// relative to its root.
gfx::Transform GetWindowTransformRelativeToRoot(
    aura::Window* window,
    const gfx::Transform& parent_transform_relative_to_root) {
  if (window->IsRootWindow())
    return gfx::Transform();

  // Concatenate |parent_transform_relative_to_root| to the result of
  // GetTargetTransformRelativeTo(parent_layer) rather than simply calling
  // GetTargetTransformRelativeTo(window->GetRootWindow()->layer()) to avoid
  // doing the same computations multiple times when traversing a window
  // hierarchy.
  ui::Layer* parent_layer = window->parent()->layer();
  gfx::Transform transform_relative_to_root;
  bool success = window->layer()->GetTargetTransformRelativeTo(
      parent_layer, &transform_relative_to_root);
  DCHECK(success);
  transform_relative_to_root.ConcatTransform(parent_transform_relative_to_root);
  return transform_relative_to_root;
}

// Returns the bounds of |window| relative to its |root|.
// |transform_relative_to_root| is the transform of |window| relative to its
// root. If |clipped_bounds| is not null, the returned bounds are clipped by it.
SkIRect GetWindowBoundsInRootWindow(
    aura::Window* window,
    const gfx::Transform& transform_relative_to_root,
    const SkIRect* clipped_bounds) {
  DCHECK(transform_relative_to_root.Preserves2dAxisAlignment());
  // Compute the unclipped bounds of |window|.
  gfx::RectF bounds(0.0f, 0.0f, static_cast<float>(window->bounds().width()),
                    static_cast<float>(window->bounds().height()));
  transform_relative_to_root.TransformRect(&bounds);
  SkIRect skirect_bounds = SkIRect::MakeXYWH(
      gfx::ToFlooredInt(bounds.x()), gfx::ToFlooredInt(bounds.y()),
      gfx::ToFlooredInt(bounds.width()), gfx::ToFlooredInt(bounds.height()));
  // If necessary, clip the bounds.
  if (clipped_bounds && !skirect_bounds.intersect(*clipped_bounds))
    return SkIRect::MakeEmpty();
  return skirect_bounds;
}

// Returns true iff the occlusion states in |tracked_windows| match those
// returned by Window::occlusion_state().
bool OcclusionStatesMatch(
    const base::flat_map<Window*, Window::OcclusionState>& tracked_windows) {
  for (const auto& tracked_window : tracked_windows) {
    if (tracked_window.second != tracked_window.first->occlusion_state())
      return false;
  }
  return true;
}

}  // namespace

WindowOcclusionTracker::ScopedPause::ScopedPause(Env* env) : env_(env) {
  env_->PauseWindowOcclusionTracking();
}

WindowOcclusionTracker::ScopedPause::~ScopedPause() {
  env_->UnpauseWindowOcclusionTracking();
}

void WindowOcclusionTracker::Track(Window* window) {
  DCHECK(window);
  DCHECK(window != window->GetRootWindow());

  auto insert_result =
      tracked_windows_.insert({window, Window::OcclusionState::UNKNOWN});
  DCHECK(insert_result.second);
  if (!window_observer_.IsObserving(window))
    window_observer_.Add(window);
  if (window->GetRootWindow())
    TrackedWindowAddedToRoot(window);
}

WindowOcclusionTracker::WindowOcclusionTracker() = default;

WindowOcclusionTracker::~WindowOcclusionTracker() = default;

void WindowOcclusionTracker::MaybeComputeOcclusion() {
  if (num_pause_occlusion_tracking_ ||
      num_times_occlusion_recomputed_in_current_step_ != 0) {
    return;
  }

  base::AutoReset<int> auto_reset(
      &num_times_occlusion_recomputed_in_current_step_, 0);

  // Recompute occlusion states until either:
  // - They are stable, i.e. calling Window::SetOcclusionState() on all tracked
  //   windows does not provoke changes that could affect occlusion.
  // - Occlusion states have been recomputed
  // |kMaxComputeOcclusionIterationsBeforeStable|
  //   times.
  // If occlusion states have been recomputed
  // |kMaxComputeOcclusionIterationsBeforeStable| times and are still not
  // stable, iterate one last time to set the occlusion state of all tracked
  // windows based on IsVisible().
  while (num_times_occlusion_recomputed_in_current_step_ <=
         kMaxRecomputeOcclusion) {
    const bool exceeded_max_num_times_occlusion_recomputed =
        num_times_occlusion_recomputed_in_current_step_ ==
        kMaxRecomputeOcclusion;
    bool found_dirty_root = false;

    // Compute occlusion states and store them in |tracked_windows_|. Do not
    // call Window::SetOcclusionState() in this phase to prevent changes to the
    // window tree while it is being traversed.
    for (auto& root_window_pair : root_windows_) {
      if (root_window_pair.second.dirty) {
        found_dirty_root = true;
        root_window_pair.second.dirty = false;
        if (!exceeded_max_num_times_occlusion_recomputed) {
          SkRegion occluded_region;
          RecomputeOcclusionImpl(root_window_pair.first, gfx::Transform(),
                                 nullptr, &occluded_region);
        }
      }
    }

    if (!found_dirty_root)
      break;

    ++num_times_occlusion_recomputed_;
    ++num_times_occlusion_recomputed_in_current_step_;

    // Call Window::SetOcclusionState() on tracked windows. A WindowDelegate may
    // change the window tree in response to this.
    WindowTracker tracked_windows_list;
    for (const auto& tracked_window : tracked_windows_)
      tracked_windows_list.Add(tracked_window.first);

    while (!tracked_windows_list.windows().empty()) {
      Window* window = tracked_windows_list.Pop();
      auto it = tracked_windows_.find(window);
      if (it != tracked_windows_.end() &&
          it->second != Window::OcclusionState::UNKNOWN) {
        // Fallback to VISIBLE/HIDDEN if the maximum number of times that
        // occlusion can be recomputed was exceeded.
        if (exceeded_max_num_times_occlusion_recomputed) {
          it->second = window->IsVisible() ? Window::OcclusionState::VISIBLE
                                           : Window::OcclusionState::HIDDEN;
        }

        window->SetOcclusionState(it->second);
      }
    }
  }

  // Sanity check: Occlusion states in |tracked_windows_| should match those
  // returned by Window::occlusion_state().
  DCHECK(OcclusionStatesMatch(tracked_windows_));
}

bool WindowOcclusionTracker::RecomputeOcclusionImpl(
    Window* window,
    const gfx::Transform& parent_transform_relative_to_root,
    const SkIRect* clipped_bounds,
    SkRegion* occluded_region) {
  DCHECK(window);

  if (!window->IsVisible()) {
    SetWindowAndDescendantsAreOccluded(window, true);
    return false;
  }

  if (WindowIsAnimated(window)) {
    SetWindowAndDescendantsAreOccluded(window, false);
    return true;
  }

  // Compute window bounds.
  const gfx::Transform transform_relative_to_root =
      GetWindowTransformRelativeToRoot(window,
                                       parent_transform_relative_to_root);
  if (!transform_relative_to_root.Preserves2dAxisAlignment()) {
    // For simplicity, windows that are not axis-aligned are considered
    // unoccluded and do not occlude other windows.
    SetWindowAndDescendantsAreOccluded(window, false);
    return true;
  }
  const SkIRect window_bounds = GetWindowBoundsInRootWindow(
      window, transform_relative_to_root, clipped_bounds);

  // Compute children occlusion states.
  const SkIRect* clipped_bounds_for_children =
      window->layer()->GetMasksToBounds() ? &window_bounds : clipped_bounds;
  bool has_visible_child = false;
  for (auto* child : base::Reversed(window->children())) {
    has_visible_child |=
        RecomputeOcclusionImpl(child, transform_relative_to_root,
                               clipped_bounds_for_children, occluded_region);
  }

  // Compute window occlusion state.
  if (occluded_region->contains(window_bounds)) {
    SetOccluded(window, !has_visible_child);
    return has_visible_child;
  }

  SetOccluded(window, false);
  if (VisibleWindowIsOpaque(window))
    occluded_region->op(window_bounds, SkRegion::kUnion_Op);
  return true;
}

bool WindowOcclusionTracker::VisibleWindowIsOpaque(Window* window) const {
  DCHECK(window->IsVisible());
  DCHECK(window->layer());
  return !window->transparent() && WindowHasContent(window) &&
         window->layer()->GetCombinedOpacity() == 1.0f &&
         // For simplicity, a shaped window is not considered opaque.
         !WindowOrParentHasShape(window);
}

bool WindowOcclusionTracker::WindowHasContent(Window* window) const {
  if (window->layer()->type() != ui::LAYER_NOT_DRAWN)
    return true;

  if (window_has_content_callback_)
    return window_has_content_callback_.Run(window);

  return false;
}

void WindowOcclusionTracker::CleanupAnimatedWindows() {
  base::EraseIf(animated_windows_, [=](Window* window) {
    ui::LayerAnimator* const animator = window->layer()->GetAnimator();
    if (animator->IsAnimatingOnePropertyOf(kSkipWindowWhenPropertiesAnimated))
      return false;
    animator->RemoveObserver(this);
    auto root_window_state_it = root_windows_.find(window->GetRootWindow());
    if (root_window_state_it != root_windows_.end())
      MarkRootWindowAsDirty(&root_window_state_it->second);
    return true;
  });
}

bool WindowOcclusionTracker::MaybeObserveAnimatedWindow(Window* window) {
  // MaybeObserveAnimatedWindow() is called when OnWindowBoundsChanged(),
  // OnWindowTransformed() or OnWindowOpacitySet() is called with
  // ui::PropertyChangeReason::FROM_ANIMATION. Despite that, if the animation is
  // ending, the IsAnimatingOnePropertyOf() call below may return false. It is
  // important not to register an observer in that case because it would never
  // be notified.
  ui::LayerAnimator* const animator = window->layer()->GetAnimator();
  if (animator->IsAnimatingOnePropertyOf(kSkipWindowWhenPropertiesAnimated)) {
    const auto insert_result = animated_windows_.insert(window);
    if (insert_result.second) {
      animator->AddObserver(this);
      return true;
    }
  }
  return false;
}

void WindowOcclusionTracker::SetWindowAndDescendantsAreOccluded(
    Window* window,
    bool is_occluded) {
  SetOccluded(window, is_occluded);
  for (Window* child_window : window->children())
    SetWindowAndDescendantsAreOccluded(child_window, is_occluded);
}

void WindowOcclusionTracker::SetOccluded(Window* window, bool is_occluded) {
  auto tracked_window = tracked_windows_.find(window);
  if (tracked_window == tracked_windows_.end())
    return;

  if (!window->IsVisible())
    tracked_window->second = Window::OcclusionState::HIDDEN;
  else if (is_occluded)
    tracked_window->second = Window::OcclusionState::OCCLUDED;
  else
    tracked_window->second = Window::OcclusionState::VISIBLE;
}

bool WindowOcclusionTracker::WindowIsTracked(Window* window) const {
  return base::ContainsKey(tracked_windows_, window);
}

bool WindowOcclusionTracker::WindowIsAnimated(Window* window) const {
  return base::ContainsKey(animated_windows_, window);
}

template <typename Predicate>
void WindowOcclusionTracker::MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(
    Window* window,
    Predicate predicate) {
  Window* root_window = window->GetRootWindow();
  if (!root_window)
    return;
  auto root_window_state_it = root_windows_.find(root_window);

  // This may be called if a WindowObserver or a LayoutManager changes |window|
  // after Window::AddChild() has added it to a new root but before
  // OnWindowAddedToRootWindow() is called on |this|. In that case, do nothing
  // here and rely on OnWindowAddedToRootWindow() to mark the new root as dirty.
  if (root_window_state_it == root_windows_.end()) {
    DCHECK(WindowIsTracked(window));
    return;
  }

  if (root_window_state_it->second.dirty)
    return;
  if (predicate()) {
    MarkRootWindowAsDirty(&root_window_state_it->second);
    MaybeComputeOcclusion();
  }
}

void WindowOcclusionTracker::MarkRootWindowAsDirty(
    RootWindowState* root_window_state) {
  // If a root window is marked as dirty and occlusion states have already been
  // recomputed |kMaxRecomputeOcclusion| times, it means that they are not
  // stabilizing.
  DCHECK_LT(num_times_occlusion_recomputed_in_current_step_,
            kMaxRecomputeOcclusion);

  root_window_state->dirty = true;
}

bool WindowOcclusionTracker::WindowOrParentIsAnimated(Window* window) const {
  while (window && !WindowIsAnimated(window))
    window = window->parent();
  return window != nullptr;
}

bool WindowOcclusionTracker::WindowOrDescendantIsTrackedAndVisible(
    Window* window) const {
  if (!window->IsVisible())
    return false;
  if (WindowIsTracked(window))
    return true;
  for (Window* child_window : window->children()) {
    if (WindowOrDescendantIsTrackedAndVisible(child_window))
      return true;
  }
  return false;
}

bool WindowOcclusionTracker::WindowOrDescendantIsOpaque(
    Window* window,
    bool assume_parent_opaque,
    bool assume_window_opaque) const {
  const bool parent_window_is_opaque =
      assume_parent_opaque || !window->parent() ||
      window->parent()->layer()->GetCombinedOpacity() == 1.0f;
  const bool window_is_opaque =
      parent_window_is_opaque &&
      (assume_window_opaque || window->layer()->opacity() == 1.0f);

  if (!window->IsVisible() || !window->layer() || !window_is_opaque ||
      WindowIsAnimated(window)) {
    return false;
  }
  if (!window->transparent() && WindowHasContent(window))
    return true;
  for (Window* child_window : window->children()) {
    if (WindowOrDescendantIsOpaque(child_window, true))
      return true;
  }
  return false;
}

bool WindowOcclusionTracker::WindowOpacityChangeMayAffectOcclusionStates(
    Window* window) const {
  // Changing the opacity of a window has no effect on the occlusion state of
  // the window or its children. It can however affect the occlusion state of
  // other windows in the tree if it is visible and not animated (animated
  // windows aren't considered in occlusion computations).
  return window->IsVisible() && !WindowOrParentIsAnimated(window);
}

bool WindowOcclusionTracker::WindowMoveMayAffectOcclusionStates(
    Window* window) const {
  return !WindowOrParentIsAnimated(window) &&
         (WindowOrDescendantIsOpaque(window) ||
          WindowOrDescendantIsTrackedAndVisible(window));
}

void WindowOcclusionTracker::TrackedWindowAddedToRoot(Window* window) {
  Window* const root_window = window->GetRootWindow();
  DCHECK(root_window);
  RootWindowState& root_window_state = root_windows_[root_window];
  ++root_window_state.num_tracked_windows;
  if (root_window_state.num_tracked_windows == 1)
    AddObserverToWindowAndDescendants(root_window);
  MarkRootWindowAsDirty(&root_window_state);
  MaybeComputeOcclusion();
}

void WindowOcclusionTracker::TrackedWindowRemovedFromRoot(Window* window) {
  Window* const root_window = window->GetRootWindow();
  DCHECK(root_window);
  auto root_window_state_it = root_windows_.find(root_window);
  DCHECK(root_window_state_it != root_windows_.end());
  --root_window_state_it->second.num_tracked_windows;
  if (root_window_state_it->second.num_tracked_windows == 0) {
    RemoveObserverFromWindowAndDescendants(root_window);
    root_windows_.erase(root_window_state_it);
  }
}

void WindowOcclusionTracker::RemoveObserverFromWindowAndDescendants(
    Window* window) {
  if (WindowIsTracked(window)) {
    DCHECK(window_observer_.IsObserving(window));
  } else {
    if (window_observer_.IsObserving(window))
      window_observer_.Remove(window);
    window->layer()->GetAnimator()->RemoveObserver(this);
    animated_windows_.erase(window);
  }
  for (Window* child_window : window->children())
    RemoveObserverFromWindowAndDescendants(child_window);
}

void WindowOcclusionTracker::AddObserverToWindowAndDescendants(Window* window) {
  if (WindowIsTracked(window)) {
    DCHECK(window_observer_.IsObserving(window));
  } else {
    DCHECK(!window_observer_.IsObserving(window));
    window_observer_.Add(window);
  }
  for (Window* child_window : window->children())
    AddObserverToWindowAndDescendants(child_window);
}

void WindowOcclusionTracker::Pause() {
  ++num_pause_occlusion_tracking_;
}

void WindowOcclusionTracker::Unpause() {
  --num_pause_occlusion_tracking_;
  DCHECK_GE(num_pause_occlusion_tracking_, 0);
  MaybeComputeOcclusion();
}

void WindowOcclusionTracker::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  CleanupAnimatedWindows();
  MaybeComputeOcclusion();
}

void WindowOcclusionTracker::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  CleanupAnimatedWindows();
  MaybeComputeOcclusion();
}

void WindowOcclusionTracker::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {}

void WindowOcclusionTracker::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  Window* const window = params.target;
  Window* const root_window = window->GetRootWindow();
  if (root_window && base::ContainsKey(root_windows_, root_window) &&
      !window_observer_.IsObserving(window)) {
    AddObserverToWindowAndDescendants(window);
  }
}

void WindowOcclusionTracker::OnWindowAdded(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(
      window, [=]() { return WindowMoveMayAffectOcclusionStates(window); });
}

void WindowOcclusionTracker::OnWillRemoveWindow(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=]() {
    return !WindowOrParentIsAnimated(window) &&
           WindowOrDescendantIsOpaque(window);
  });
}

void WindowOcclusionTracker::OnWindowVisibilityChanged(Window* window,
                                                       bool visible) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=]() {
    // A child isn't visible when its parent isn't IsVisible(). Therefore, there
    // is no need to compute occlusion when Show() or Hide() is called on a
    // window with a hidden parent.
    return (!window->parent() || window->parent()->IsVisible()) &&
           !WindowOrParentIsAnimated(window);
  });
}

void WindowOcclusionTracker::OnWindowBoundsChanged(
    Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  // Call MaybeObserveAnimatedWindow() outside the lambda so that the window can
  // be marked as animated even when its root is dirty.
  const bool animation_started =
      (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
      MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=]() {
    return animation_started || WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowOpacitySet(
    Window* window,
    ui::PropertyChangeReason reason) {
  // Call MaybeObserveAnimatedWindow() outside the lambda so that the window can
  // be marked as animated even when its root is dirty.
  const bool animation_started =
      (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
      MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=]() {
    return animation_started ||
           WindowOpacityChangeMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowAlphaShapeSet(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=]() {
    return WindowOpacityChangeMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowTransformed(
    Window* window,
    ui::PropertyChangeReason reason) {
  // Call MaybeObserveAnimatedWindow() outside the lambda so that the window can
  // be marked as animated even when its root is dirty.
  const bool animation_started =
      (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
      MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=]() {
    return animation_started || WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowStackingChanged(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(
      window, [=]() { return WindowMoveMayAffectOcclusionStates(window); });
}

void WindowOcclusionTracker::OnWindowDestroyed(Window* window) {
  DCHECK(!window->GetRootWindow() || (window == window->GetRootWindow()));
  tracked_windows_.erase(window);
  window_observer_.Remove(window);
  // Animations should be completed or aborted before a window is destroyed.
  DCHECK(!window->layer()->GetAnimator()->IsAnimatingOnePropertyOf(
      kSkipWindowWhenPropertiesAnimated));
  // |window| must be removed from |animated_windows_| to prevent an invalid
  // access in CleanupAnimatedWindows() if |window| is being destroyed from a
  // LayerAnimationObserver after an animation has ended but before |this| has
  // been notified.
  animated_windows_.erase(window);
}

void WindowOcclusionTracker::OnWindowAddedToRootWindow(Window* window) {
  DCHECK(window->GetRootWindow());
  if (WindowIsTracked(window))
    TrackedWindowAddedToRoot(window);
}

void WindowOcclusionTracker::OnWindowRemovingFromRootWindow(Window* window,
                                                            Window* new_root) {
  DCHECK(window->GetRootWindow());
  if (WindowIsTracked(window))
    TrackedWindowRemovedFromRoot(window);
  RemoveObserverFromWindowAndDescendants(window);
}

void WindowOcclusionTracker::OnWindowLayerRecreated(Window* window) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();

  // Recreating the layer may have stopped animations.
  if (animator->IsAnimatingOnePropertyOf(kSkipWindowWhenPropertiesAnimated))
    return;

  size_t num_removed = animated_windows_.erase(window);
  if (num_removed == 0)
    return;

  animator->RemoveObserver(this);
  auto root_window_state_it = root_windows_.find(window->GetRootWindow());
  if (root_window_state_it != root_windows_.end()) {
    MarkRootWindowAsDirty(&root_window_state_it->second);
    MaybeComputeOcclusion();
  }
}

}  // namespace aura
