// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_SCOPED_PAINT_LOCK_H_
#define UI_VIEWS_SCOPED_PAINT_LOCK_H_

#include "base/scoped_observation.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// A scoped lock that prevents a View and its entire subtree from being painted
// for the lifetime of the lock.
class VIEWS_EXPORT ScopedPaintLock : public ViewObserver {
 public:
  explicit ScopedPaintLock(View* view);

  ScopedPaintLock(const ScopedPaintLock&) = delete;
  ScopedPaintLock& operator=(const ScopedPaintLock&) = delete;

  ~ScopedPaintLock() override;

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

 private:
  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_SCOPED_PAINT_LOCK_H_
