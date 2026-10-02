// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_VIEW_BOUNDS_AND_VISIBILITY_INPUT_PROTECTION_POLICY_H_
#define UI_VIEWS_INPUT_PROTECTION_VIEW_BOUNDS_AND_VISIBILITY_INPUT_PROTECTION_POLICY_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/input_protection/input_protection_policy.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class InputEventActivationProtector;

// An implementation of `InputProtectionPolicy` that protects a given `View`
// against unintended interaction by observing changes to its bounds and
// visibility. The `View` must already belong to a `Widget` when the policy is
// constructed (e.g. in `View::AddedToWidget()`).
//
// It blocks input events targeting the observed `View` or any descendant of the
// observed `View` in the following cases:
//
// 1. Bounds change: The observed `View` changes position or size within the
//    containing `Widget` (either directly or because an ancestor view moved)
//    while drawn in a visible `Widget`. Changes from empty bounds (such as
//    initial layout) do not trigger protection.
// 2. Visibility change: The observed `View` or its containing `Widget` becomes
//    visible while the `View` is drawn (including when constructed for a drawn
//    `View` in an already visible `Widget`).
class VIEWS_EXPORT ViewBoundsAndVisibilityInputProtectionPolicy final
    : public InputProtectionPolicy,
      public ViewObserver {
 public:
  explicit ViewBoundsAndVisibilityInputProtectionPolicy(View& view);
  ~ViewBoundsAndVisibilityInputProtectionPolicy() override;

  // InputProtectionPolicy:
  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      const InputEventActivationProtector& protector) override;
  void OnProtectionStarted() override;
  void OnProtectionReset() override;
  void OnProtectionStopped() override;

  // ViewObserver:
  void OnViewVisibleBoundsChanged(View* observed_view) override;
  void OnViewRemovedFromWidget(View* observed_view) override;
  void OnViewVisibilityChanged(View* observed_view,
                               View* starting_view,
                               bool visible) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  // Returns `true` if the observed `View` is drawn and attached to a visible
  // `Widget`.
  bool IsViewDrawnAndWidgetVisible() const;

  // The last recorded bounds of the observed view in widget coordinates.
  gfx::Rect last_bounds_;

  // The timestamp when the view was last protected.
  base::TimeTicks view_protected_time_stamp_;

  // Enables `OnViewVisibleBoundsChanged` notifications when the observed `View`
  // or any ancestor view changes bounds.
  std::unique_ptr<View::ScopedNotifyObserversOnVisibleBoundsChanged>
      scoped_notify_visible_bounds_changed_;

  // Observation of the target `View`.
  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_VIEW_BOUNDS_AND_VISIBILITY_INPUT_PROTECTION_POLICY_H_
