// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_animation_observer.h"

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace ui {
namespace {
// Setting to false disable the check globally.
bool default_check_active_duration = true;
}  // namespace

// Do not fail on builds that run slow, such as SANITIZER, debug.
#if !DCHECK_IS_ON() || defined(ADDRESS_SANITIZER) ||           \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) ||  \
    defined(LEAK_SANITIZER) || defined(UNDEFINED_SANITIZER) || \
    !defined(NDEBUG)
#define NOTREACHED_OR_WARN() LOG(WARNING)
#else
#define NOTREACHED_OR_WARN() NOTREACHED()
#endif

// Log animations that took more than 1m.  When DCHECK is enabled, it will fail
// with DCHECK error.
constexpr base::TimeDelta kThreshold = base::Minutes(1);

CompositorAnimationObserver::CompositorAnimationObserver(
    const base::Location& location)
    : location_(location) {}

CompositorAnimationObserver::~CompositorAnimationObserver() = default;

void CompositorAnimationObserver::Start() {
  if (default_check_active_duration && check_active_duration_)
    start_.emplace(base::TimeTicks::Now());
}

void CompositorAnimationObserver::Check() {
  if (start_ && (base::TimeTicks::Now() - *start_ > kThreshold)) {
    NotifyFailure();
    start_.reset();
  }
}

void CompositorAnimationObserver::ResetIfActive() {
  if (start_)
    start_.emplace(base::TimeTicks::Now());
}

void CompositorAnimationObserver::NotifyFailure() {
  if (!base::debug::BeingDebugged() &&
      !base::subtle::ScopedTimeClockOverrides::overrides_active()) {
    NOTREACHED_OR_WARN()
        << "CompositorAnimationObserver is active for too long ("
        << (base::TimeTicks::Now() - *start_).InSecondsF()
        << "s) location=" << location_.ToString();
  }
}

void CompositorAnimationObserver::DisableCheckActiveDuration() {
  default_check_active_duration = false;
}

}  // namespace ui
