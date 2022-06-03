// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_
#define UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_

#include "base/location.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Compositor;

class COMPOSITOR_EXPORT CompositorAnimationObserver {
 public:
  explicit CompositorAnimationObserver(
      const base::Location& location = FROM_HERE);
  virtual ~CompositorAnimationObserver();

  virtual void OnAnimationStep(base::TimeTicks timestamp) = 0;
  virtual void OnCompositingShuttingDown(Compositor* compositor) = 0;

  void Start();
  void Check();
  void ResetIfActive();

  bool is_active_for_test() const { return !!start_; }

 protected:
  virtual void NotifyFailure();

 private:
  base::Location location_;
  absl::optional<base::TimeTicks> start_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_
