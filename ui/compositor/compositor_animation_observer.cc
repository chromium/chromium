// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_animation_observer.h"

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/trace_event.h"

namespace ui {
namespace {
// Setting to false disable the check globally.
bool default_check_active_duration = true;
}  // namespace

// This warning should only be fatal on non-official DCHECK builds that are not
// known to be runtime-slow. Slow builds for this purpose are debug builds
// (!NDEBUG) and any sanitizer build.
#if !DCHECK_IS_ON() || defined(OFFICIAL_BUILD) || !defined(NDEBUG) || \
    defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) ||        \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||           \
    defined(UNDEFINED_SANITIZER)
#define DFATAL_OR_WARNING WARNING
#else
#define DFATAL_OR_WARNING DFATAL
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
    TRACE_EVENT_BEGIN("ui", "LongCompositorAnimationObserved",
                      perfetto::ThreadTrack::Current(), *start_);
    TRACE_EVENT_END("ui");
    LOG(DFATAL_OR_WARNING)
        << "CompositorAnimationObserver is active for too long ("
        << (base::TimeTicks::Now() - *start_).InSecondsF()
        << "s) location=" << location_.ToString();
  }
}

void CompositorAnimationObserver::DisableCheckActiveDuration() {
  default_check_active_duration = false;
}

}  // namespace ui
