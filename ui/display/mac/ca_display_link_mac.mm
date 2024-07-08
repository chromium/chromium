// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/ca_display_link_mac.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CADisplayLink.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "ui/display/mac/display_link_mac.h"

namespace {
// Implement HW VSync with CADisplayLink. If CADisplayLink is not available or
// fails, it fallbacks to CVDisplayLink.
BASE_FEATURE(kCADisplayLink,
             "CADisplayLink",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

struct CADisplayLinkWrapper::ObjCState {
  CADisplayLink* __strong display_link API_AVAILABLE(macos(14.0));
  CADisplayLinkTarget* __strong target API_AVAILABLE(macos(14.0));
  NSScreen* ns_screen;
};

// static
std::unique_ptr<CADisplayLinkWrapper> CADisplayLinkWrapper::Create(
    CGDirectDisplayID display_id,
    DisplayLinkCallback display_link_callback) {
  if (!base::FeatureList::IsEnabled(kCADisplayLink)) {
    return nullptr;
  }

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

    // Pause CVDisplaylink callback until a request for start.
    objc_state->display_link.paused = YES;

    // Set the default refresh rate
    float refresh_rate = 1.0 / objc_state->ns_screen.minimumRefreshInterval;
    [objc_state->display_link
        setPreferredFrameRateRange:CAFrameRateRange{.minimum = refresh_rate,
                                                    .maximum = refresh_rate,
                                                    .preferred = refresh_rate}];

    [objc_state->display_link addToRunLoop:NSRunLoop.currentRunLoop
                                   forMode:NSDefaultRunLoopMode];

    std::unique_ptr<CADisplayLinkWrapper> wrapper(
        new CADisplayLinkWrapper(std::move(objc_state)));

    // Set the CADisplayLinkTarget's callback to call back into the C++ code.
    [wrapper->objc_state_->target
        setCallback:base::BindRepeating(&CADisplayLinkWrapper::Step,
                                        base::Unretained(wrapper.get()))];

    wrapper->display_id_ = display_id;
    wrapper->callback_ = display_link_callback;
    wrapper->min_interval_ =
        base::Seconds(1) *
        wrapper->objc_state_->ns_screen.minimumRefreshInterval;
    wrapper->max_interval_ = wrapper->min_interval_;
    wrapper->preferred_interval_ = wrapper->min_interval_;

    return wrapper;
  }
  return nullptr;
}

CADisplayLinkWrapper::CADisplayLinkWrapper(
    std::unique_ptr<ObjCState> objc_state) {
  if (@available(macos 14.0, *)) {
    objc_state_ = std::move(objc_state);
  }
}

CADisplayLinkWrapper::~CADisplayLinkWrapper() {
  // We must manually invalidate the CADisplayLink as its addToRunLoop keeps
  // strong reference to its target. Thus, releasing our wrapper won't really
  // result in destroying the object.
  if (@available(macos 14.0, *)) {
    DCHECK(objc_state_);
    DCHECK(objc_state_->display_link);

    [objc_state_->target setCallback:base::RepeatingClosure()];
    [objc_state_->display_link invalidate];
    objc_state_->display_link = nil;
  }
}

void CADisplayLinkWrapper::Start() {
  if (@available(macos 14.0, *)) {
    if (!paused_) {
      return;
    }
    paused_ = false;
    objc_state_->display_link.paused = NO;
  }
}

void CADisplayLinkWrapper::Stop() {
  if (@available(macos 14.0, *)) {
    if (paused_) {
      return;
    }
    paused_ = true;
    objc_state_->display_link.paused = YES;
  }
}

double CADisplayLinkWrapper::GetRefreshRate() {
  if (@available(macos 12.0, *)) {
    return 1.0 / objc_state_->ns_screen.minimumRefreshInterval;
  }
  return 0;
}

void CADisplayLinkWrapper::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) {
  if (@available(macos 12.0, *)) {
    min_interval =
        base::Seconds(1) * objc_state_->ns_screen.minimumRefreshInterval;
    max_interval =
        base::Seconds(1) * objc_state_->ns_screen.maximumRefreshInterval;
    granularity =
        base::Seconds(1) * objc_state_->ns_screen.displayUpdateGranularity;
  }
}

void CADisplayLinkWrapper::SetPreferredIntervalRange(
    base::TimeDelta min_interval,
    base::TimeDelta max_interval,
    base::TimeDelta preferred_interval) {
  if (@available(macos 14.0, *)) {
    // Sanity check for the order.
    DCHECK(preferred_interval <= max_interval &&
           preferred_interval >= min_interval);

    // The |preferred_interval| must be a supported interval if a fixed refresh
    // rate is requested, otherwise CVDisplayLink terminates app due to uncaught
    // exception 'NSInvalidArgumentException', reason: 'invalid range'.
    if (min_interval == max_interval && min_interval == preferred_interval) {
      preferred_interval = AdjustedToSupportedInterval(preferred_interval);
    }

    base::TimeDelta ns_screen_min_interval =
        base::Seconds(1) * objc_state_->ns_screen.minimumRefreshInterval;
    base::TimeDelta ns_screen_max_interval =
        base::Seconds(1) * objc_state_->ns_screen.maximumRefreshInterval;

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
    if (preferred_interval_ == preferred_interval &&
        max_interval_ == max_interval && min_interval_ == min_interval) {
      return;
    }

    min_interval_ = min_interval;
    max_interval_ = max_interval;
    preferred_interval_ = preferred_interval;

    float min_refresh_rate = base::Seconds(1) / max_interval;
    float max_refresh_rate = base::Seconds(1) / min_interval;
    float preferred_refresh_rate = base::Seconds(1) / preferred_interval;
    [objc_state_->display_link
        setPreferredFrameRateRange:CAFrameRateRange{
                                       .minimum = min_refresh_rate,
                                       .maximum = max_refresh_rate,
                                       .preferred = preferred_refresh_rate}];
  }
}

void CADisplayLinkWrapper::Step() {
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

    callback_.Run(params);
  }
}

base::TimeDelta CADisplayLinkWrapper::AdjustedToSupportedInterval(
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

}  // namespace ui
