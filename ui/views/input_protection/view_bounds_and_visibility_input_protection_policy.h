// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_VIEW_BOUNDS_AND_VISIBILITY_INPUT_PROTECTION_POLICY_H_
#define UI_VIEWS_INPUT_PROTECTION_VIEW_BOUNDS_AND_VISIBILITY_INPUT_PROTECTION_POLICY_H_

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/views/input_protection/input_protection_policy.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class InputEventActivationProtector;

// An implementation of `InputProtectionPolicy` that protects a given `View`
// against unintended interaction by observing changes to its visibility. The
// `View` must already belong to a `Widget` when the policy is constructed
// (e.g. in `View::AddedToWidget()`).
//
// It blocks input events targeting the observed `View` or any descendant of the
// observed `View` when the observed `View` or its containing `Widget` becomes
// visible while the `View` is drawn (including when constructed for a drawn
// `View` in an already visible `Widget`).
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
  void OnViewRemovedFromWidget(View* observed_view) override;
  void OnViewVisibilityChanged(View* observed_view,
                               View* starting_view,
                               bool visible) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  // Returns `true` if the observed `View` is drawn and attached to a visible
  // `Widget`.
  bool IsViewDrawnAndWidgetVisible() const;

  // The timestamp when the view was last protected.
  base::TimeTicks view_protected_time_stamp_;

  // Observation of the target `View`.
  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_VIEW_BOUNDS_AND_VISIBILITY_INPUT_PROTECTION_POLICY_H_
