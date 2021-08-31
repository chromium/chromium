// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_animation_observer.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace ui {

#if DCHECK_IS_ON()
#define NOTREACHED_OR_WARN() NOTREACHED()
#else
#define NOTREACHED_OR_WARN() LOG(WARNING)
#endif

// Log animations that took more than 15s.  When DCHECK is enabled, it will fail
// with DCHECK error.
constexpr base::TimeDelta kThreshold = base::TimeDelta::FromMilliseconds(15000);

CompositorAnimationObserver::CompositorAnimationObserver(
    const base::Location& location)
    : location_(location) {}

CompositorAnimationObserver::~CompositorAnimationObserver() = default;

void CompositorAnimationObserver::Start() {
  start_.emplace(base::TimeTicks::Now());
}

void CompositorAnimationObserver::Check() {
  if (start_ && (base::TimeTicks::Now() - *start_ > kThreshold)) {
    NOTREACHED_OR_WARN()
        << "CompositorAnimationObserver is active for too long ("
        << (base::TimeTicks::Now() - *start_).InSecondsF()
        << "s) location=" << location_.ToString();
    start_.reset();
  }
}

}  // namespace ui
