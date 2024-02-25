// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_
#define UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_

#include <optional>

#include "base/location.h"
#include "base/time/time.h"
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

  // This must be set before adding to the compositor.
  void set_check_active_duration(bool check_active_duration) {
    check_active_duration_ = check_active_duration;
  }

  // Disables
  static void DisableCheckActiveDuration();

 protected:
  virtual void NotifyFailure();

 private:
  bool check_active_duration_ = true;
  base::Location location_;
  std::optional<base::TimeTicks> start_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_
