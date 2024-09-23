// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_occlusion_tracker.h"

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/env.h"
#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/aura/window_occlusion_change_builder.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace aura {

namespace {

// When one of these properties is animated, a window is considered non-occluded
// and cannot occlude other windows.
// TODO(crbug.com/40677173): Mark a window VISIBLE when COLOR animation starts.
constexpr ui::LayerAnimationElement::AnimatableProperties
    kSkipWindowWhenPropertiesAnimated =
        ui::LayerAnimationElement::TRANSFORM |
        ui::LayerAnimationElement::BOUNDS | ui::LayerAnimationElement::OPACITY |
        ui::LayerAnimationElement::COLOR;

// When an animation ends for one of these properties, occlusion states might
// be affected. The end of an animation for a property in
// |kSkipWindowWhenPropertiesAnimated| might affect occlusion states because
// a window suddenly stops being excluded from occlusion computations. The
// end of a visibility animation might affect occlusion states because a
// window is suddenly considered drawn/not drawn.
constexpr ui::LayerAnimationElement::AnimatableProperties
    kOcclusionCanChangeWhenPropertyAnimationEnds =
        kSkipWindowWhenPropertiesAnimated |
        ui::LayerAnimationElement::VISIBILITY;

// Maximum number of times that MaybeComputeOcclusion() should have to recompute
// occlusion states before they become stable.
//
// TODO(fdoray): This can be changed to 2 once showing/hiding a WebContents
// doesn't cause a call to Show()/Hide() on the Window of a
// RenderWidgetHostViewAura. https://crbug.com/827268
constexpr int kMaxRecomputeOcclusion = 3;

bool WindowOrParentHasShape(const Window* window) {
  if (window->layer()->alpha_shape())
    return true;
  if (window->parent())
    return WindowOrParentHasShape(window->parent());
  return false;
}

bool WindowHasOpaqueRegionsForOcclusion(const Window* window) {
  return !window->opaque_regions_for_occlusion().empty();
}

// Returns the transform of |window| relative to its root.
// |parent_transform_relative_to_root| is the transform of |window->parent()|
// relative to its root.
gfx::Transform GetWindowTransformRelativeToRoot(
    Window* window,
    const gfx::Transform& parent_transform_relative_to_root,
    bool use_target_values) {
  if (window->IsRootWindow())
    return gfx::Transform();

  // Compute the transform relative to root by concatenating the transform
  // of this layer and the transform of the parent relative to root.
  // If |use_target_values| is true, use the target bounds and transform instead
  // of the true values.
  gfx::Transform translation;
  gfx::Transform transform_relative_to_root;
  if (use_target_values) {
    translation.Translate(
        static_cast<float>(window->layer()->GetTargetBounds().x()),
        static_cast<float>(window->layer()->GetTargetBounds().y()));
    transform_relative_to_root = window->layer()->GetTargetTransform();
  } else {
    translation.Translate(static_cast<float>(window->layer()->bounds().x()),
                          static_cast<float>(window->layer()->bounds().y()));
    transform_relative_to_root = window->layer()->transform();
  }
  transform_relative_to_root.PostConcat(translation);
  transform_relative_to_root.PostConcat(parent_transform_relative_to_root);
  return transform_relative_to_root;
}

// Applies `transform_relative_to_root` to `bounds` and returns the enclosing
// bounds.
SkIRect ComputeTransformedBoundsEnclosing(
    const gfx::Rect& bounds,
    const gfx::Transform& transform_relative_to_root) {
  DCHECK(transform_relative_to_root.Preserves2dAxisAlignment());
  return gfx::RectToSkIRect(transform_relative_to_root.MapRect(bounds));
}

// Applies `transform_relative_to_root` to `bounds` and returns the enclosed
// bounds.
SkIRect ComputeTransformedBoundsEnclosed(
    const gfx::Rect& bounds,
    const gfx::Transform& transform_relative_to_root) {
  DCHECK(transform_relative_to_root.Preserves2dAxisAlignment());
  return gfx::RectToSkIRect(gfx::ToEnclosedRect(
      transform_relative_to_root.MapRect(gfx::RectF(bounds))));
}

SkIRect ComputeClippedBounds(SkIRect bounds, const SkIRect* clipped_bounds) {
  // If necessary, clip the bounds.
  if (clipped_bounds && !bounds.intersect(*clipped_bounds)) {
    return SkIRect::MakeEmpty();
  }
  return bounds;
}

// Returns the bounds of |window| relative to its |root|.
// |transform_relative_to_root| is the transform of |window| relative to its
// root. If |clipped_bounds| is not null, the returned bounds are clipped by it.
// If the bounds after transform have fractional coordinates, enclosed bounds in
// integers are used.
SkIRect GetWindowBoundsInRootWindow(
    Window* window,
    const gfx::Transform& transform_relative_to_root,
    const SkIRect* clipped_bounds,
    bool use_target_values) {
  // Compute the unclipped bounds of |window|.
  const gfx::Rect src_bounds =
      use_target_values ? window->layer()->GetTargetBounds() : window->bounds();
  const SkIRect transformed_bounds = ComputeTransformedBoundsEnclosed(
      gfx::Rect(src_bounds.size()), transform_relative_to_root);
  return ComputeClippedBounds(transformed_bounds, clipped_bounds);
}

// Returns the bounds that |window| should contribute to be used for occluding
// other windows. This is different to the bounds of the window if |window|
// has opaque regions for occlusion set. We need to use different sets of bounds
// for computing the occlusion of a window itself versus what it should
// contribute to occluding other windows because a translucent region should
// not be considered to occlude other windows, but must be covered by something
// opaque for it itself to be occluded.
// If the bounds after transform have fractional coordinates, enclosing bounds
// in integers are used.
SkIRect GetOpaqueBoundsInRootWindow(
    Window* window,
    const gfx::Transform& transform_relative_to_root,
    const SkIRect* clipped_bounds) {
  DCHECK(WindowHasOpaqueRegionsForOcclusion(window));
  // TODO: Currently, we only support one Rect in the opaque region.
  DCHECK_EQ(1u, window->opaque_regions_for_occlusion().size());

  // Don't let clients mark regions outside their window bounds as opaque.
  // Note: opaque_regions_for_occlusion() are relative to the window, i.e. the
  // top-left corner of the window is considered to be the point (0, 0).
  gfx::Rect opaque_region = window->opaque_regions_for_occlusion()[0];
  opaque_region.Intersect(gfx::Rect(window->bounds().size()));
  const SkIRect transformed_bounds = ComputeTransformedBoundsEnclosing(
      opaque_region, transform_relative_to_root);
  return ComputeClippedBounds(transformed_bounds, clipped_bounds);
}

float GetLayerCombinedTargetOpacity(const ui::Layer* layer) {
  float opacity = layer->GetTargetOpacity();
  const ui::Layer* current = layer->parent();
  while (current) {
    opacity *= current->GetTargetOpacity();
    current = current->parent();
  }
  return opacity;
}

}  // namespace

WindowOcclusionTracker::InnerClient::InnerClient(
    WindowOcclusionTracker* occlusion_tracker)
    : occlusion_tracker_(
          occlusion_tracker ? occlusion_tracker
                            : Env::GetInstance()->GetWindowOcclusionTracker()) {
  CHECK(occlusion_tracker_);
}

WindowOcclusionTracker::InnerClient::~InnerClient() = default;

WindowOcclusionTracker::ScopedPause::ScopedPause(
    WindowOcclusionTracker* occlusion_tracker)
    : InnerClient(occlusion_tracker) {
  occlusion_tracker_->Pause();
}

WindowOcclusionTracker::ScopedPause::~ScopedPause() {
  occlusion_tracker_->Unpause();
}

WindowOcclusionTracker::ScopedExclude::ScopedExclude(
    Window* window,
    WindowOcclusionTracker* occlusion_tracker)
    : InnerClient(occlusion_tracker), window_(window) {
  window->AddObserver(this);
  occlusion_tracker_->Exclude(window_);
}

WindowOcclusionTracker::ScopedExclude::~ScopedExclude() {
  Shutdown();
}

void WindowOcclusionTracker::ScopedExclude::OnWindowDestroying(Window* window) {
  DCHECK_EQ(window_, window);
  Shutdown();
}

void WindowOcclusionTracker::ScopedExclude::Shutdown() {
  if (window_) {
    window_->RemoveObserver(this);
    occlusion_tracker_->Unexclude(window_);
    window_ = nullptr;
    occlusion_tracker_ = nullptr;
  }
}

WindowOcclusionTracker::ScopedForceVisible::ScopedForceVisible(
    Window* window,
    WindowOcclusionTracker* occlusion_tracker)
    : InnerClient(occlusion_tracker), window_(window) {
  window_->AddObserver(this);
  occlusion_tracker_->ForceWindowVisible(window_);
}

WindowOcclusionTracker::ScopedForceVisible::~ScopedForceVisible() {
  Shutdown();
}

void WindowOcclusionTracker::ScopedForceVisible::OnWindowDestroying(
    Window* window) {
  DCHECK_EQ(window_, window);
  Shutdown();
}

void WindowOcclusionTracker::ScopedForceVisible::Shutdown() {
  if (window_) {
    window_->RemoveObserver(this);
    occlusion_tracker_->RemoveForceWindowVisible(window_);
    window_ = nullptr;
    occlusion_tracker_ = nullptr;
  }
}

void WindowOcclusionTracker::Track(Window* window) {
  DCHECK(window);
  DCHECK(window != window->GetRootWindow());

  auto insert_result = tracked_windows_.insert({window, {}});
  if (!insert_result.second)
    return;

  if (!window_observations_.IsObservingSource(window))
    window_observations_.AddObservation(window);
  if (window->GetRootWindow())
    TrackedWindowAddedToRoot(window);
}

WindowOcclusionTracker::OcclusionData
WindowOcclusionTracker::ComputeTargetOcclusionForWindow(Window* window) {
  // Compute the occlusion with target state, just for this window.
  // This doesn't update the occlusion states of any window, so we should only
  // require one pass.
  auto tracked_window_iter = tracked_windows_.find(window);
  CHECK(tracked_window_iter != tracked_windows_.end(),
        base::NotFatalUntil::M130);

  base::AutoReset<OcclusionData> auto_reset_occlusion_data(
      &tracked_window_iter->second, OcclusionData());
  DCHECK(!target_occlusion_window_);
  base::AutoReset<raw_ptr<Window>> auto_reset_target_occlusion_window(
      &target_occlusion_window_, window);

  Window* root_window = window->GetRootWindow();
  SkRegion occluded_region;
  SkIRect root_window_clip = gfx::RectToSkIRect(root_window->bounds());
  RecomputeOcclusionImpl(root_window, gfx::Transform(), &root_window_clip,
                         &occluded_region);

  return tracked_window_iter->second;
}

WindowOcclusionTracker::WindowOcclusionTracker() = default;

WindowOcclusionTracker::~WindowOcclusionTracker() = default;

bool WindowOcclusionTracker::OcclusionStatesMatch(
    const base::flat_map<Window*, OcclusionData>& tracked_windows) {
  for (const auto& tracked_window : tracked_windows) {
    if (tracked_window.second.occlusion_state !=
        tracked_window.first->GetOcclusionState())
      return false;
  }
  return true;
}

void WindowOcclusionTracker::MaybeComputeOcclusion() {
  if (num_pause_occlusion_tracking_ ||
      num_times_occlusion_recomputed_in_current_step_ != 0) {
    return;
  }

  base::AutoReset<int> auto_reset(
      &num_times_occlusion_recomputed_in_current_step_, 0);

  TRACE_EVENT1("ui", "WindowOcclusionTracker::MaybeComputeOcclusion", "this",
               reinterpret_cast<void*>(this));

  // Recompute occlusion states until either:
  // - They are stable, i.e. calling Window::SetOcclusionInfo() on all tracked
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
    // call Window::SetOcclusionInfo() in this phase to prevent changes to the
    // window tree while it is being traversed.
    for (auto& root_window_pair : root_windows_) {
      if (root_window_pair.second.dirty) {
        found_dirty_root = true;
        root_window_pair.second.dirty = false;
        if (!exceeded_max_num_times_occlusion_recomputed) {
          Window* root_window = root_window_pair.first;
          if (root_window_pair.second.occlusion_state ==
              Window::OcclusionState::OCCLUDED) {
            SetWindowAndDescendantsAreOccluded(
                root_window, /* is_occluded */ true, root_window->IsVisible());
// TODO(crbug.com/40262710): Enable for other platforms in a separate CL.
#if BUILDFLAG(IS_CHROMEOS)
          } else if (root_window_pair.second.occlusion_state ==
                     Window::OcclusionState::HIDDEN) {
            SetWindowAndDescendantsAreOccluded(root_window,
                                               /* is_occluded */ false,
                                               /* is_parent_visible */ false);
#endif
          } else {
            SkRegion occluded_region = root_window_pair.second.occluded_region;
            SkIRect root_window_clip =
                gfx::RectToSkIRect(root_window->bounds());
            RecomputeOcclusionImpl(root_window, gfx::Transform(),
                                   &root_window_clip, &occluded_region);
          }
        }
      }
    }

    if (!found_dirty_root)
      break;

    ++num_times_occlusion_recomputed_;
    ++num_times_occlusion_recomputed_in_current_step_;

    std::unique_ptr<WindowOcclusionChangeBuilder> change_builder =
        occlusion_change_builder_factory_
            ? occlusion_change_builder_factory_.Run()
            : WindowOcclusionChangeBuilder::Create();
    for (auto& it : tracked_windows_) {
      Window* window = it.first;
      if (it.second.occlusion_state == Window::OcclusionState::UNKNOWN)
        continue;

      // Fallback to VISIBLE/HIDDEN if the maximum number of times that
      // occlusion can be recomputed was exceeded.
      if (exceeded_max_num_times_occlusion_recomputed) {
        if (WindowIsVisible(window))
          it.second.occlusion_state = Window::OcclusionState::VISIBLE;
        else
          it.second.occlusion_state = Window::OcclusionState::HIDDEN;
        it.second.occluded_region = SkRegion();
      }

      change_builder->Add(window, it.second.occlusion_state,
                          it.second.occluded_region);
    }
  }

  // Sanity check: Occlusion states in |tracked_windows_| should match those
  // returned by Window::GetOcclusionState() if the default
  // `WindowOcclusionChangeBuilder` is being used.
  DCHECK(occlusion_change_builder_factory_ ||
         OcclusionStatesMatch(tracked_windows_));
}

bool WindowOcclusionTracker::RecomputeOcclusionImpl(
    Window* window,
    const gfx::Transform& parent_transform_relative_to_root,
    const SkIRect* clipped_bounds,
    SkRegion* occluded_region) {
  DCHECK(window);

  const bool force_visible = WindowIsForcedVisible(window);
  // This does not use Window::IsVisible() as that returns the wrong thing for
  // any ancestors that are forced visible.
  const bool is_visible =
      force_visible ||
      (ShouldUseTargetValues() ? window->layer()->GetTargetVisibility()
                               : window->layer()->visible());
  if (!is_visible) {
    SetWindowAndDescendantsAreOccluded(window, /* is_occluded */ true,
                                       /* is_parent_visible */ true);
    return false;
  }

  // TODO: While considering that a window whose color is animated doesn't
  // occlude other windows helps reduce the number of times that occlusion is
  // recomputed, it isn't necessary to consider that the window whose color is
  // animated itself is non-occluded.
  if (WindowIsAnimated(window) || WindowIsExcluded(window)) {
    SetWindowAndDescendantsAreOccluded(window, /* is_occluded */ false,
                                       /* is_parent_visible */ true);
    return true;
  }

  // Compute window bounds.
  const gfx::Transform transform_relative_to_root =
      GetWindowTransformRelativeToRoot(
          window, parent_transform_relative_to_root, ShouldUseTargetValues());
  if (!transform_relative_to_root.Preserves2dAxisAlignment()) {
    // For simplicity, windows that are not axis-aligned are considered
    // unoccluded and do not occlude other windows.
    SetWindowAndDescendantsAreOccluded(window, /* is_occluded */ false,
                                       /* is_parent_visible */ true);
    return true;
  }

  const SkIRect window_bounds = GetWindowBoundsInRootWindow(
      window, transform_relative_to_root,
      force_visible ? nullptr : clipped_bounds, ShouldUseTargetValues());

  // Compute children occlusion states.
  const SkIRect* clipped_bounds_for_children =
      window->layer()->GetMasksToBounds() ? &window_bounds : clipped_bounds;
  bool has_visible_child = false;
  SkRegion occluded_region_before_traversing_children = *occluded_region;
  // Windows that are forced visible are always considered visible, so have no
  // clip.
  SkRegion region_for_forced_visible_windows;
  SkRegion* occluded_region_for_children =
      force_visible ? &region_for_forced_visible_windows : occluded_region;
  for (aura::Window* child : base::Reversed(window->children())) {
    has_visible_child |= RecomputeOcclusionImpl(
        child, transform_relative_to_root, clipped_bounds_for_children,
        occluded_region_for_children);
  }

  // Window is fully occluded.
  if (!force_visible && occluded_region->contains(window_bounds) &&
      !has_visible_child) {
    SetOccluded(window, /* is_occluded */ true, /* is_parent_visible */ true,
                SkRegion());
    return false;
  }

  // Window is partially occluded or unoccluded. Windows that are forced visible
  // are considered completely visible (so they get an empty SkRegion()).
  SetOccluded(
      window, false, /* is_parent_visible */ true,
      force_visible ? SkRegion() : occluded_region_before_traversing_children);

  if (!force_visible && VisibleWindowCanOccludeOtherWindows(window)) {
    const SkIRect occlusion_bounds =
        WindowHasOpaqueRegionsForOcclusion(window)
            ? GetOpaqueBoundsInRootWindow(window, transform_relative_to_root,
                                          clipped_bounds)
            : window_bounds;
    occluded_region->op(occlusion_bounds, SkRegion::kUnion_Op);
  }
  return true;
}

bool WindowOcclusionTracker::VisibleWindowCanOccludeOtherWindows(
    const Window* window) const {
  DCHECK(window->layer());
  float combined_opacity = ShouldUseTargetValues()
                               ? GetLayerCombinedTargetOpacity(window->layer())
                               : window->layer()->GetCombinedOpacity();
  // Just check the alpha on this layer as an alpha on parent solid color layers
  // will not affect children's opacity.
  if (window->layer()->type() == ui::LAYER_SOLID_COLOR) {
    auto color = ShouldUseTargetValues() ? window->layer()->GetTargetColor()
                                         : window->layer()->background_color();
    combined_opacity *= SkColorGetA(color) / 255.f;
  }
  return (!window->GetTransparent() && WindowHasContent(window) &&
          combined_opacity == 1.0f &&
          // For simplicity, a shaped window is not considered opaque.
          !WindowOrParentHasShape(window)) ||
         WindowHasOpaqueRegionsForOcclusion(window);
}

bool WindowOcclusionTracker::WindowHasContent(const Window* window) const {
  if (window->layer()->type() != ui::LAYER_NOT_DRAWN)
    return true;

  return false;
}

void WindowOcclusionTracker::CleanupAnimatedWindows() {
  base::EraseIf(animated_windows_, [=, this](Window* window) {
    ui::LayerAnimator* const animator = window->layer()->GetAnimator();
    if (animator->IsAnimatingOnePropertyOf(
            kOcclusionCanChangeWhenPropertyAnimationEnds))
      return false;

    RemoveAnimationObservationForLayer(window->layer());
    MarkRootWindowAsDirty(window->GetRootWindow());
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
  if (animator->IsAnimatingOnePropertyOf(
          kOcclusionCanChangeWhenPropertyAnimationEnds)) {
    const auto insert_result = animated_windows_.insert(window);
    if (insert_result.second) {
      layer_animator_observations.AddObservation(animator);
      animated_layer_observations_.AddObservation(window->layer());
      return true;
    }
  }
  return false;
}

void WindowOcclusionTracker::SetWindowAndDescendantsAreOccluded(
    Window* window,
    bool is_occluded,
    bool is_parent_visible) {
  const bool force_visible = WindowIsForcedVisible(window);
  const bool is_visible =
      force_visible || (is_parent_visible && window->layer()->visible());
  is_occluded = is_occluded && !force_visible;
  SetOccluded(window, is_occluded, is_visible, SkRegion());
  for (Window* child_window : window->children())
    SetWindowAndDescendantsAreOccluded(child_window, is_occluded, is_visible);
}

void WindowOcclusionTracker::SetOccluded(Window* window,
                                         bool is_occluded,
                                         bool is_parent_visible,
                                         const SkRegion& occluded_region) {
  // Don't modify occlusion state if we're just computing occlusion for one
  // window.
  if (target_occlusion_window_ != nullptr && target_occlusion_window_ != window)
    return;
  auto tracked_window = tracked_windows_.find(window);
  if (tracked_window == tracked_windows_.end())
    return;

  // Set the occluded region of the window.
  tracked_window->second.occluded_region = occluded_region;

  const bool is_visible = WindowIsForcedVisible(window) ||
                          (is_parent_visible && window->layer()->visible());
  if (!is_visible)
    tracked_window->second.occlusion_state = Window::OcclusionState::HIDDEN;
  else if (is_occluded)
    tracked_window->second.occlusion_state = Window::OcclusionState::OCCLUDED;
  else
    tracked_window->second.occlusion_state = Window::OcclusionState::VISIBLE;

  DCHECK(tracked_window->second.occlusion_state ==
             Window::OcclusionState::VISIBLE ||
         tracked_window->second.occluded_region.isEmpty());
}

bool WindowOcclusionTracker::WindowIsTracked(Window* window) const {
  return base::Contains(tracked_windows_, window);
}

bool WindowOcclusionTracker::WindowIsAnimated(Window* window) const {
  return !ShouldUseTargetValues() &&
         base::Contains(animated_windows_, window) &&
         window->layer()->GetAnimator()->IsAnimatingOnePropertyOf(
             kSkipWindowWhenPropertiesAnimated);
}

bool WindowOcclusionTracker::WindowIsExcluded(Window* window) const {
  return base::Contains(excluded_windows_, window);
}

bool WindowOcclusionTracker::WindowIsVisible(Window* window) const {
  if (forced_visible_count_map_.empty())
    return window->IsVisible();
  Window* w = window;
  Window* last = w;
  while (w) {
    if (!WindowIsForcedVisible(window)) {
      if (ShouldUseTargetValues() && !w->layer()->GetTargetVisibility())
        return false;
      if (!ShouldUseTargetValues() && !w->layer()->visible())
        return false;
    }
    last = w;
    w = w->parent();
  }
  return last->IsRootWindow();
}

bool WindowOcclusionTracker::WindowIsForcedVisible(Window* window) const {
  return forced_visible_count_map_.count(window) > 0;
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
    MarkRootWindowStateAsDirty(&root_window_state_it->second);
    MaybeComputeOcclusion();
  }
}

void WindowOcclusionTracker::MarkRootWindowStateAsDirty(
    RootWindowState* root_window_state) {
  // If a root window is marked as dirty and occlusion states have already been
  // recomputed |kMaxRecomputeOcclusion| times, it means that they are not
  // stabilizing.
  DCHECK_LT(num_times_occlusion_recomputed_in_current_step_,
            kMaxRecomputeOcclusion);

  root_window_state->dirty = true;
}

bool WindowOcclusionTracker::MarkRootWindowAsDirty(Window* root_window) {
  auto root_window_state_it = root_windows_.find(root_window);
  if (root_window_state_it == root_windows_.end())
    return false;
  MarkRootWindowStateAsDirty(&root_window_state_it->second);
  return true;
}

bool WindowOcclusionTracker::WindowOrParentIsAnimated(Window* window) const {
  while (window && !WindowIsAnimated(window))
    window = window->parent();
  return window != nullptr;
}

bool WindowOcclusionTracker::WindowOrDescendantIsTrackedAndVisible(
    Window* window) const {
  if (!WindowIsVisible(window))
    return false;
  if (WindowIsTracked(window))
    return true;
  for (Window* child_window : window->children()) {
    if (WindowOrDescendantIsTrackedAndVisible(child_window))
      return true;
  }
  return false;
}

bool WindowOcclusionTracker::WindowOrDescendantCanOccludeOtherWindows(
    Window* window,
    bool assume_parent_opaque,
    bool assume_window_opaque) const {
  const bool parent_window_is_opaque =
      assume_parent_opaque || !window->parent() ||
      window->parent()->layer()->GetCombinedOpacity() == 1.0f;
  const bool window_is_opaque =
      parent_window_is_opaque &&
      (assume_window_opaque || window->layer()->opacity() == 1.0f);

  if (!WindowIsVisible(window) || !window->layer() || !window_is_opaque ||
      WindowIsAnimated(window)) {
    return false;
  }
  if ((!window->GetTransparent() && WindowHasContent(window)) ||
      WindowHasOpaqueRegionsForOcclusion(window)) {
    return true;
  }
  for (Window* child_window : window->children()) {
    if (WindowOrDescendantCanOccludeOtherWindows(child_window, true))
      return true;
  }
  return false;
}

bool WindowOcclusionTracker::WindowOpacityChangeMayAffectOcclusionStates(
    Window* window) const {
  // Changing the opacity of a window has no effect on the occlusion state of
  // the window or its children. It can however affect the occlusion state of
  // other windows in the tree if it is visible and not animated (animated
  // windows aren't considered in occlusion computations), unless it is
  // excluded.
  return WindowIsVisible(window) && !WindowOrParentIsAnimated(window) &&
         !WindowIsExcluded(window);
}

bool WindowOcclusionTracker::WindowMoveMayAffectOcclusionStates(
    Window* window) const {
  return !WindowOrParentIsAnimated(window) && !WindowIsExcluded(window) &&
         (WindowOrDescendantCanOccludeOtherWindows(window) ||
          WindowOrDescendantIsTrackedAndVisible(window));
}

void WindowOcclusionTracker::TrackedWindowAddedToRoot(Window* window) {
  Window* const root_window = window->GetRootWindow();
  DCHECK(root_window);
  RootWindowState& root_window_state = root_windows_[root_window];
  ++root_window_state.num_tracked_windows;
  MarkRootWindowStateAsDirty(&root_window_state);

  // It's only useful to track the host if |window| is the first tracked window
  // under |root_window|.  All windows under the same root have the same host.
  if (root_window_state.num_tracked_windows == 1) {
    AddObserverToWindowAndDescendants(root_window);
    auto* host = root_window->GetHost();
    if (host) {
      window_tree_host_observations_.AddObservation(host);
      if (!NativeWindowOcclusionTracker::
              IsNativeWindowOcclusionTrackingAlwaysEnabled(host)) {
        NativeWindowOcclusionTracker::EnableNativeWindowOcclusionTracking(host);
      }
    }
  }
  MaybeComputeOcclusion();
}

void WindowOcclusionTracker::TrackedWindowRemovedFromRoot(Window* window) {
  Window* const root_window = window->GetRootWindow();
  DCHECK(root_window);
  auto root_window_state_it = root_windows_.find(root_window);
  CHECK(root_window_state_it != root_windows_.end(), base::NotFatalUntil::M130);
  --root_window_state_it->second.num_tracked_windows;
  if (root_window_state_it->second.num_tracked_windows == 0) {
    RemoveObserverFromWindowAndDescendants(root_window);
    root_windows_.erase(root_window_state_it);
    WindowTreeHost* host = root_window->GetHost();
    window_tree_host_observations_.RemoveObservation(host);

    if (!NativeWindowOcclusionTracker::
            IsNativeWindowOcclusionTrackingAlwaysEnabled(host)) {
      NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(host);
    }
  }
}

void WindowOcclusionTracker::RemoveObserverFromWindowAndDescendants(
    Window* window) {
  if (WindowIsTracked(window)) {
    DCHECK(window_observations_.IsObservingSource(window));
  } else {
    if (window_observations_.IsObservingSource(window))
      window_observations_.RemoveObservation(window);
    RemoveAnimationObservationForLayer(window->layer());
    animated_windows_.erase(window);
  }
  for (Window* child_window : window->children())
    RemoveObserverFromWindowAndDescendants(child_window);
}

void WindowOcclusionTracker::AddObserverToWindowAndDescendants(Window* window) {
  if (WindowIsTracked(window)) {
    DCHECK(window_observations_.IsObservingSource(window));
  } else {
    DCHECK(!window_observations_.IsObservingSource(window));
    window_observations_.AddObservation(window);
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

void WindowOcclusionTracker::Exclude(Window* window) {
  // If threre is a valid use case to exclude the same window twice
  // (e.g. independent clients may try to exclude the same window),
  // introduce the count.
  DCHECK(!WindowIsExcluded(window));
  excluded_windows_.insert(window);
  if (WindowIsVisible(window)) {
    if (MarkRootWindowAsDirty(window->GetRootWindow()))
      MaybeComputeOcclusion();
  }
}

void WindowOcclusionTracker::Unexclude(Window* window) {
  DCHECK(WindowIsExcluded(window));
  excluded_windows_.erase(window);
  if (WindowIsVisible(window)) {
    if (MarkRootWindowAsDirty(window->GetRootWindow()))
      MaybeComputeOcclusion();
  }
}

void WindowOcclusionTracker::ForceWindowVisible(Window* window) {
  if (forced_visible_count_map_[window]++ == 0) {
    Window* root_window = window->GetRootWindow();
    if (root_window && MarkRootWindowAsDirty(root_window))
      MaybeComputeOcclusion();
  }
}

void WindowOcclusionTracker::RemoveForceWindowVisible(Window* window) {
  auto iter = forced_visible_count_map_.find(window);
  CHECK(iter != forced_visible_count_map_.end(), base::NotFatalUntil::M130);
  if (--iter->second == 0u) {
    forced_visible_count_map_.erase(iter);
    Window* root_window = window->GetRootWindow();
    if (root_window && MarkRootWindowAsDirty(root_window))
      MaybeComputeOcclusion();
  }
}

bool WindowOcclusionTracker::ShouldUseTargetValues() const {
  return target_occlusion_window_;
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

bool WindowOcclusionTracker::RequiresNotificationWhenAnimatorDestroyed() const {
  // `OnLayerAnimationAborted()` should be called if the `LayerAnimator` is
  // destroyed while an animation is still active. This gives
  // `WindowOcclusionTracker` a chance to unregister itself as a
  // `LayerAnimationObserver`.
  return true;
}

void WindowOcclusionTracker::LayerDestroyed(ui::Layer* layer) {
  // The only known use case here is for the layer recreation. When
  // `OnWindowLayerRecreated()` is called, the `window` already has a new
  // `Layer` and `LayerAnimator`. `WindowOcclusionTracker` needs access to the
  // old layer though so that it can unregister itself as an observer of any
  // animated properties. This method should be called for the old layer.
  //
  // For all other use cases, this method is effectively a no-op because some
  // other observer event in this class fired already and caused
  // `RemoveAnimationObservationForLayer()` to be called.
  RemoveAnimationObservationForLayer(layer);
}

void WindowOcclusionTracker::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  Window* const window = params.target;
  Window* const root_window = window->GetRootWindow();
  if (root_window && base::Contains(root_windows_, root_window) &&
      !window_observations_.IsObservingSource(window)) {
    AddObserverToWindowAndDescendants(window);
  }
}

void WindowOcclusionTracker::OnWindowAdded(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWillRemoveWindow(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return !WindowOrParentIsAnimated(window) &&
           WindowOrDescendantCanOccludeOtherWindows(window);
  });
}

void WindowOcclusionTracker::OnWindowVisibilityChanged(Window* window,
                                                       bool visible) {
  MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    // A child isn't visible when its parent isn't IsVisible(). Therefore, there
    // is no need to compute occlusion when Show() or Hide() is called on a
    // window with a hidden parent.
    return (!window->parent() || WindowIsVisible(window->parent())) &&
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
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
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
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return animation_started ||
           WindowOpacityChangeMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowAlphaShapeSet(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return WindowOpacityChangeMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowTransparentChanged(
    Window* window,
    ui::PropertyChangeReason reason) {
  // Call MaybeObserveAnimatedWindow() outside the lambda so that the window can
  // be marked as animated even when its root is dirty.
  const bool animation_started =
      (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
      MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return animation_started ||
           WindowOpacityChangeMayAffectOcclusionStates(window);
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
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return animation_started || WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowStackingChanged(Window* window) {
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowDestroyed(Window* window) {
  DCHECK(!window->GetRootWindow() || (window == window->GetRootWindow()));
  tracked_windows_.erase(window);
  window_observations_.RemoveObservation(window);
  // Animations should be completed or aborted before a window is destroyed.
  DCHECK(!window->layer()->GetAnimator()->IsAnimatingOnePropertyOf(
      kOcclusionCanChangeWhenPropertyAnimationEnds));
  // |window| must be removed from |animated_windows_| to prevent an invalid
  // access in CleanupAnimatedWindows() if |window| is being destroyed from a
  // LayerAnimationObserver after an animation has ended but before |this| has
  // been notified.
  RemoveAnimationObservationForLayer(window->layer());
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
  if (animator->IsAnimatingOnePropertyOf(
          kOcclusionCanChangeWhenPropertyAnimationEnds))
    return;

  size_t num_removed = animated_windows_.erase(window);
  if (num_removed == 0)
    return;

  if (MarkRootWindowAsDirty(window->GetRootWindow()))
    MaybeComputeOcclusion();
}

void WindowOcclusionTracker::OnWindowOpaqueRegionsForOcclusionChanged(
    Window* window) {
  // If the opaque regions for occlusion change, the occlusion state may be
  // affected if the effective opacity of the window changes (e.g. clearing the
  // regions for occlusion), or if their bounds change.
  MarkRootWindowAsDirtyAndMaybeComputeOcclusionIf(window, [=, this]() {
    return WindowOpacityChangeMayAffectOcclusionStates(window) ||
           WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnOcclusionStateChanged(
    WindowTreeHost* host,
    Window::OcclusionState new_state,
    const SkRegion& occluded_region) {
  Window* root_window = host->window();
  auto root_window_state_it = root_windows_.find(root_window);
  if (root_window_state_it == root_windows_.end())
    return;

  root_window_state_it->second.occlusion_state = new_state;
  root_window_state_it->second.occluded_region = occluded_region;

  MarkRootWindowAsDirty(root_window);
  MaybeComputeOcclusion();
}

void WindowOcclusionTracker::RemoveAnimationObservationForLayer(
    ui::Layer* layer) {
  if (animated_layer_observations_.IsObservingSource(layer)) {
    animated_layer_observations_.RemoveObservation(layer);
  }

  ui::LayerAnimator* const animator = layer->GetAnimator();
  if (layer_animator_observations.IsObservingSource(animator)) {
    layer_animator_observations.RemoveObservation(animator);
  }
}

}  // namespace aura
