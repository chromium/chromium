// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_GUARD_H_
#define UI_VIEWS_VIEW_GUARD_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

// Ensures that the given view does not cease to exist during the lifetime of
// the guard object. Used to enforce invariants in the codebase where it is
// better to crash than to risk memory corruption - i.e. when conceivably some
// callback *could* result in the view's destruction but no *reasonable*
// circumstance should actually allow it.
//
// Similar to `ViewTracker` except that instead of setting the internal
// reference to null on destruction, this class CHECK()s.
//
// Similar to raw_ref and raw_ptr in that they guard against UAFs with process
// death, but ViewGuard provides immediate protection against unexpected
// destruction even in deeply-nested calls - there's literally no time to
// dereference an invalid pointer since the protected object is valid all the
// way up until the CHECK().
//
// Example:
// ```
//  // There are synchronous callbacks that can occur during setup that could
//  // potentially occur during this operation, but they should realistically
//  // never result in the destruction of the new View.
//  auto v = std::make_unique<MyView>();
//  ViewGuard guard(*v);
//  v->SetProperty(...);
//  v_ = AddChildView(std::move(v));
//  v_ -> ...;
// ```
class VIEWS_EXPORT ViewGuard : public ViewObserver {
 public:
  explicit ViewGuard(View& View);
  ViewGuard(const ViewGuard& other);
  ViewGuard& operator=(const ViewGuard& other) = delete;
  ~ViewGuard() override;

 private:
  // ViewObserver:
  void OnViewIsDeleting(View* view) override;

  const raw_ref<View> view_;
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_GUARD_H_
