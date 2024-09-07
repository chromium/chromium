// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/ca_display_link_mac.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CADisplayLink.h>
#include <stdint.h>

#include <map>
#include <set>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"

namespace {

NSScreen* GetNSScreenFromDisplayID(CGDirectDisplayID display_id) {
  for (NSScreen* screen in NSScreen.screens) {
    CGDirectDisplayID screenNumber =
        [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
    if (screenNumber == display_id) {
      return screen;
    }
  }

  return nullptr;
}

}  // namespace

API_AVAILABLE(macos(14.0))
@interface CADisplayLinkTarget : NSObject {
  base::RepeatingClosure _callback;
}
- (void)step:(CADisplayLink*)displayLink;
- (void)setCallback:(base::RepeatingClosure)callback;
@end

@implementation CADisplayLinkTarget
- (void)step:(CADisplayLink*)displayLink {
  DCHECK(_callback);
  _callback.Run();
}

- (void)setCallback:(base::RepeatingClosure)callback {
  _callback = callback;
}

@end

namespace ui {

namespace {
struct CADisplayLinkGlobals {
  std::map<std::pair<CGDirectDisplayID, base::PlatformThreadId>,
           std::unique_ptr<CASharedState>>
      GUARDED_BY(lock) map;

  // The lock for updating the map.
  base::Lock lock;

  static CADisplayLinkGlobals& Get() {
    static base::NoDestructor<CADisplayLinkGlobals> instance;
    return *instance;
  }
};
}  // namespace

// CASharedState is always accessed on the same thread.
class CASharedState {
 public:
  static std::unique_ptr<CASharedState> Create(
      CGDirectDisplayID display_id,
      base::PlatformThreadId thread_id);

  struct ObjCState {
    CADisplayLink* __strong display_link API_AVAILABLE(macos(14.0));
    CADisplayLinkTarget* __strong target API_AVAILABLE(macos(14.0));
    NSScreen* ns_screen;
  };

  explicit CASharedState(std::unique_ptr<ObjCState> objc_state,
                         CGDirectDisplayID display_id,
                         base::PlatformThreadId thread_id);
  CASharedState(const CASharedState&) = delete;
  CASharedState& operator=(const CASharedState&) = delete;

  ~CASharedState();

  void Retain();
  void Release();

  void Start();
  void Stop();

  void StopDisplayLinkIfNeeded();

  // CADisplayLink callback.
  void Step();

  uint32_t refcount_ = 0;

  std::unique_ptr<ObjCState> objc_state_;

  const CGDirectDisplayID display_id_;
  const base::PlatformThreadId thread_id_;

  bool paused_ = true;

  // The system can change the available range of frame rates because it factors
  // in system policies and a person’s preferences. For example, Low Power Mode,
  // critical thermal state, and accessibility settings can affect the system’s
  // frame rate. The system typically provides a consistent frame rate by
  // choosing one that’s a factor of the display’s maximum refresh rate.

  // The current frame interval range set in CADisplayLink
  // setPreferredFrameRateRange().
  base::TimeDelta preferred_interval_;
  base::TimeDelta max_interval_;
  base::TimeDelta min_interval_;

  // The number of consecutive DisplayLink VSyncs received after zero
  // |callbacks_|. DisplayLink will be stopped after |kMaxExtraVSyncs| is
  // reached. It's guarded by |globals.lock|.
  int consecutive_vsyncs_with_no_callbacks_ = 0;

  std::set<VSyncCallbackMac*> callbacks_;
  base::WeakPtrFactory<CASharedState> weak_factory_{this};
};

// static
std::unique_ptr<CASharedState> CASharedState::Create(
    CGDirectDisplayID display_id,
    base::PlatformThreadId thread_id) {
  if (@available(macos 14.0, *)) {
    auto objc_state = std::make_unique<ObjCState>();
    objc_state->ns_screen = GetNSScreenFromDisplayID(display_id);
    if (!objc_state->ns_screen) {
      return nullptr;
    }

    objc_state->target = [[CADisplayLinkTarget alloc] init];
    objc_state->display_link =
        [objc_state->ns_screen displayLinkWithTarget:objc_state->target
                                            selector:@selector(step:)];

    if (!objc_state->display_link) {
      return nullptr;
    }

    // Pause CADisplaylink callback until a request for start.
    objc_state->display_link.paused = YES;

    // Set the default refresh rate
    float refresh_rate = 1.0 / objc_state->ns_screen.minimumRefreshInterval;
    [objc_state->display_link
        setPreferredFrameRateRange:CAFrameRateRange{.minimum = refresh_rate,
                                                    .maximum = refresh_rate,
                                                    .preferred = refresh_rate}];

    [objc_state->display_link addToRunLoop:NSRunLoop.currentRunLoop
                                   forMode:NSDefaultRunLoopMode];

    std::unique_ptr<CASharedState> shared_state(
        new CASharedState(std::move(objc_state), display_id, thread_id));

    // Set the CADisplayLinkTarget's callback to call back into the C++ code.
    [shared_state->objc_state_->target
        setCallback:base::BindRepeating(
                        &CASharedState::Step,
                        shared_state->weak_factory_.GetWeakPtr())];

    shared_state->min_interval_ =
        base::Seconds(1) *
        shared_state->objc_state_->ns_screen.minimumRefreshInterval;
    shared_state->max_interval_ = shared_state->min_interval_;
    shared_state->preferred_interval_ = shared_state->min_interval_;

    return shared_state;
  }
  return nullptr;
}

CASharedState::CASharedState(std::unique_ptr<ObjCState> objc_state,
                             CGDirectDisplayID display_id,
                             base::PlatformThreadId thread_id)
    : display_id_(display_id), thread_id_(thread_id) {
  if (@available(macos 14.0, *)) {
    objc_state_ = std::move(objc_state);
  }
}

CASharedState::~CASharedState() {
  // We must manually invalidate the CADisplayLink as its addToRunLoop keeps
  // strong reference to its target. Thus, releasing our shared_state won't
  // really result in destroying the object.
  if (@available(macos 14.0, *)) {
    DCHECK(objc_state_);
    DCHECK(objc_state_->display_link);

    [objc_state_->display_link invalidate];
    objc_state_->display_link = nil;
  }
}

void CASharedState::Retain() {
  refcount_++;
}

void CASharedState::Release() {
  DCHECK(refcount_ > 0);
  refcount_ -= 1;

  // Remove this CASharedState from globals.
  if (refcount_ == 0) {
    auto& globals = CADisplayLinkGlobals::Get();
    base::AutoLock lock(globals.lock);
    auto found = globals.map.find({display_id_, thread_id_});
    DCHECK(found != globals.map.end());
    DCHECK(found->second.get() == this);

    // If the reference count drops to zero, the populate `scoped_this` with
    // the std::unique_ptr holding `this`, so that it can be deleted after
    // `globals.lock` is released.
    std::unique_ptr<CASharedState> scoped_this = std::move(found->second);
    globals.map.erase(found);

    // Let `scoped_this` be destroyed now that `globals.lock` is no longer held.
    scoped_this = nullptr;
  }
}

void CASharedState::Step() {
  TRACE_EVENT0("ui", "CADisplayLinkCallback");

  if (@available(macos 14.0, *)) {
    base::TimeTicks callbackTime =
        base::TimeTicks() + base::Seconds(objc_state_->display_link.timestamp);
    base::TimeTicks nextCallbackTime =
        base::TimeTicks() +
        base::Seconds(objc_state_->display_link.targetTimestamp);

    bool callback_times_valid = true;
    bool display_times_valid = true;
    base::TimeDelta current_interval = nextCallbackTime - callbackTime;

    // Sanity check.
    if (callbackTime.is_null() || nextCallbackTime.is_null() ||
        !current_interval.is_positive()) {
      callback_times_valid = false;
      display_times_valid = false;
    }

    ui::VSyncParamsMac params;
    params.callback_times_valid = callback_times_valid;
    params.callback_timebase = callbackTime;
    params.callback_interval = current_interval;

    params.display_times_valid = display_times_valid;
    params.display_timebase = nextCallbackTime + 0.5 * current_interval;
    params.display_interval = current_interval;

    // Issue all of its callbacks.
    // UnregisterCallback() might be called while running the callbacks.
    auto callbacks = callbacks_;
    for (auto* callback : callbacks) {
      callback->callback_for_displaylink_thread_.Run(params);
    }

    // Check if it's time to stop CADisplayLink after extra callbacks.
    StopDisplayLinkIfNeeded();
  }
}

void CASharedState::StopDisplayLinkIfNeeded() {
  if (!callbacks_.empty()) {
    consecutive_vsyncs_with_no_callbacks_ = 0;
    return;
  }

  // Allow extra callbacks before comletely stops.
  consecutive_vsyncs_with_no_callbacks_ += 1;
  if (consecutive_vsyncs_with_no_callbacks_ <
      VSyncCallbackMac::kMaxExtraVSyncs) {
    return;
  }

  if (paused_) {
    return;
  }

  consecutive_vsyncs_with_no_callbacks_ = 0;
  Stop();
}

void CASharedState::Start() {
  if (@available(macos 14.0, *)) {
    if (!paused_) {
      return;
    }
    paused_ = false;
    objc_state_->display_link.paused = NO;
  }
}

void CASharedState::Stop() {
  if (@available(macos 14.0, *)) {
    if (paused_) {
      return;
    }
    paused_ = true;
    objc_state_->display_link.paused = YES;
  }
}

////////////////////////////////////////////////////////////////////////////////
// CADisplayLinkMac

double CADisplayLinkMac::GetRefreshRate() const {
  if (@available(macos 12.0, *)) {
    return 1.0 / shared_state_->objc_state_->ns_screen.minimumRefreshInterval;
  }
  return 0;
}

void CADisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  if (@available(macos 12.0, *)) {
    min_interval = base::Seconds(1) *
                   shared_state_->objc_state_->ns_screen.minimumRefreshInterval;
    max_interval = base::Seconds(1) *
                   shared_state_->objc_state_->ns_screen.maximumRefreshInterval;
    granularity =
        base::Seconds(1) *
        shared_state_->objc_state_->ns_screen.displayUpdateGranularity;
  }
}

void CADisplayLinkMac::SetPreferredInterval(base::TimeDelta interval) {
  return SetPreferredIntervalRange(interval, interval, interval);
}

void CADisplayLinkMac::SetPreferredIntervalRange(
    base::TimeDelta min_interval,
    base::TimeDelta max_interval,
    base::TimeDelta preferred_interval) {
  if (@available(macos 14.0, *)) {
    // Sanity check for the order.
    DCHECK(preferred_interval <= max_interval &&
           preferred_interval >= min_interval);

    // The |preferred_interval| must be a supported interval if a fixed refresh
    // rate is requested, otherwise CADisplayLink terminates app due to uncaught
    // exception 'NSInvalidArgumentException', reason: 'invalid range'.
    if (min_interval == max_interval && min_interval == preferred_interval) {
      preferred_interval = AdjustedToSupportedInterval(preferred_interval);
    }

    base::TimeDelta ns_screen_min_interval =
        base::Seconds(1) *
        shared_state_->objc_state_->ns_screen.minimumRefreshInterval;
    base::TimeDelta ns_screen_max_interval =
        base::Seconds(1) *
        shared_state_->objc_state_->ns_screen.maximumRefreshInterval;

    // Cap the intervals to the upper bound and the lower bound.
    if (max_interval > ns_screen_max_interval) {
      max_interval = ns_screen_max_interval;
    }
    if (min_interval < ns_screen_min_interval) {
      min_interval = ns_screen_min_interval;
    }
    if (preferred_interval > max_interval) {
      preferred_interval = max_interval;
    }
    if (preferred_interval < min_interval) {
      preferred_interval = min_interval;
    }

    // No interval changes.
    if (shared_state_->preferred_interval_ == preferred_interval &&
        shared_state_->max_interval_ == max_interval &&
        shared_state_->min_interval_ == min_interval) {
      return;
    }

    shared_state_->min_interval_ = min_interval;
    shared_state_->max_interval_ = max_interval;
    shared_state_->preferred_interval_ = preferred_interval;

    float min_refresh_rate = base::Seconds(1) / max_interval;
    float max_refresh_rate = base::Seconds(1) / min_interval;
    float preferred_refresh_rate = base::Seconds(1) / preferred_interval;
    [shared_state_->objc_state_->display_link
        setPreferredFrameRateRange:CAFrameRateRange{
                                       .minimum = min_refresh_rate,
                                       .maximum = max_refresh_rate,
                                       .preferred = preferred_refresh_rate}];
  }
}

base::TimeDelta CADisplayLinkMac::AdjustedToSupportedInterval(
    base::TimeDelta interval) {
  base::TimeDelta min_interval;
  base::TimeDelta max_interval;
  base::TimeDelta granularity;
  GetRefreshIntervalRange(min_interval, max_interval, granularity);

  // The screen supports any update rate between the minimum and maximum refresh
  // intervals if granularity is 0.
  if (granularity.is_zero()) {
    return interval;
  }

  auto multiplier = std::round((interval - min_interval) / granularity);
  base::TimeDelta target_interval = min_interval + granularity * multiplier;

  if (target_interval <= min_interval) {
    return min_interval;
  }
  if (target_interval >= max_interval) {
    return max_interval;
  }

  return target_interval;
}

// static
scoped_refptr<CADisplayLinkMac> CADisplayLinkMac::GetForDisplayOnCurrentThread(
    CGDirectDisplayID display_id) {
  // CADisplayLink is created per display and per thread.
  auto thread_id = base::PlatformThreadBase::CurrentId();
  std::pair<CGDirectDisplayID, base::PlatformThreadId> map_id(display_id,
                                                              thread_id);

  auto& globals = CADisplayLinkGlobals::Get();
  base::AutoLock lock(globals.lock);
  auto found = globals.map.find(map_id);
  if (found != globals.map.end()) {
    return new CADisplayLinkMac(found->second.get());
  }

  auto shared_state = CASharedState::Create(display_id, thread_id);
  if (!shared_state) {
    return nullptr;
  }
  found = globals.map.emplace(map_id, std::move(shared_state)).first;

  return new CADisplayLinkMac(found->second.get());
}

base::TimeTicks CADisplayLinkMac::GetCurrentTime() const {
  return base::TimeTicks();
}

CADisplayLinkMac::CADisplayLinkMac(CASharedState* shared_state)
    : shared_state_(shared_state) {
  shared_state_->Retain();
}

CADisplayLinkMac::~CADisplayLinkMac() {
  // `shared_state_` may be deleted by the call to Release. Avoid dangling
  // raw_ptr warnings by setting `shared_state_` to nullptr prior to calling
  // Release.
  CASharedState* shared_state = shared_state_;
  shared_state_ = nullptr;

  shared_state->Release();
  shared_state = nullptr;
}

std::unique_ptr<VSyncCallbackMac> CADisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback) {
  // Make CADisplayLink callbacks to run on the same RUNLOOP of the register
  // thread without PostTask accross threads.
  std::unique_ptr<VSyncCallbackMac> new_callback(new VSyncCallbackMac(
      base::BindOnce(&CADisplayLinkMac::UnregisterCallback, this),
      std::move(callback), /*post_callback_to_ctor_thread=*/false));
  shared_state_->callbacks_.insert(new_callback.get());

  // Ensure that CADisplayLink is running.
  shared_state_->Start();

  return new_callback;
}

void CADisplayLinkMac::UnregisterCallback(VSyncCallbackMac* callback) {
  shared_state_->callbacks_.erase(callback);
}

}  // namespace ui
