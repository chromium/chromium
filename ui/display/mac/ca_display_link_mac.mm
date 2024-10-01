// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/ca_display_link_mac.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CADisplayLink.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
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
API_AVAILABLE(macos(14.0))
ui::VSyncParamsMac ComputeVSyncParametersMac(CADisplayLink* display_link) {
  // The time interval that represents when the last frame displayed.
  base::TimeTicks callback_time =
      base::TimeTicks() + base::Seconds(display_link.timestamp);
  // The time interval that represents when the next frame displays.
  base::TimeTicks next_callback_time =
      base::TimeTicks() + base::Seconds(display_link.targetTimestamp);

  bool times_valid = true;
  base::TimeDelta current_interval = next_callback_time - callback_time;

  // Sanity check.
  if (callback_time.is_null() || next_callback_time.is_null() ||
      !current_interval.is_positive()) {
    times_valid = false;
  }

  ui::VSyncParamsMac params;
  params.callback_times_valid = times_valid;
  params.callback_timebase = callback_time;
  params.callback_interval = current_interval;

  params.display_times_valid = times_valid;
  params.display_timebase = next_callback_time + 0.5 * current_interval;
  params.display_interval = current_interval;

  return params;
}
}  // namespace

struct ObjCState {
  CADisplayLink* __strong display_link API_AVAILABLE(macos(14.0));
  CADisplayLinkTarget* __strong target API_AVAILABLE(macos(14.0));
  NSScreen* ns_screen;
};

void CADisplayLinkMac::Step() {
  TRACE_EVENT0("ui", "CADisplayLinkCallback");

  if (@available(macos 14.0, *)) {
    // Allow extra callbacks before stopping CADisplayLink.
    if (!vsync_callback_) {
      consecutive_vsyncs_with_no_callbacks_ += 1;
      if (consecutive_vsyncs_with_no_callbacks_ >=
          VSyncCallbackMac::kMaxExtraVSyncs) {
        // It's time to stop CADisplayLink.
        objc_state_->display_link.paused = YES;
      }
      return;
    }

    consecutive_vsyncs_with_no_callbacks_ = 0;

    ui::VSyncParamsMac params =
        ComputeVSyncParametersMac(objc_state_->display_link);

    // UnregisterCallback() might be called while running the callbacks.
    vsync_callback_->callback_for_displaylink_thread_.Run(params);
  }
}

double CADisplayLinkMac::GetRefreshRate() const {
  if (@available(macos 12.0, *)) {
    return 1.0 / objc_state_->ns_screen.minimumRefreshInterval;
  }
  return 0;
}

void CADisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  if (@available(macos 12.0, *)) {
    min_interval =
        base::Seconds(1) * objc_state_->ns_screen.minimumRefreshInterval;
    max_interval =
        base::Seconds(1) * objc_state_->ns_screen.maximumRefreshInterval;
    granularity =
        base::Seconds(1) * objc_state_->ns_screen.displayUpdateGranularity;
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
scoped_refptr<DisplayLinkMac> CADisplayLinkMac::GetForDisplayOnCurrentThread(
    CGDirectDisplayID display_id) {
  if (@available(macos 14.0, *)) {
    scoped_refptr<CADisplayLinkMac> display_link(
        new CADisplayLinkMac(display_id));
    auto* objc_state = display_link->objc_state_.get();

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

    objc_state->display_link.preferredFrameRateRange =
        CAFrameRateRange{.minimum = refresh_rate,
                         .maximum = refresh_rate,
                         .preferred = refresh_rate};

    // This display link interface requires the task executor of the current
    // thread (CrGpuMain or VizCompositorThread) to run with
    // MessagePumpType::NS_RUNLOOP. CADisplayLinkTarget and display_link are
    // NSObject, and MessagePumpType::DEFAULT does not support system work.
    // There will be no callbacks (CADisplayLinkTarget::step()) at all if
    // MessagePumpType NS_RUNLOOP is not chosen during thread initialization.
    [objc_state->display_link addToRunLoop:NSRunLoop.currentRunLoop
                                   forMode:NSDefaultRunLoopMode];

    // Set the CADisplayLinkTarget's callback to call back into the C++ code.
    [objc_state->target
        setCallback:base::BindRepeating(
                        &CADisplayLinkMac::Step,
                        display_link->weak_factory_.GetWeakPtr())];

    display_link->min_interval_ =
        base::Seconds(1) * objc_state->ns_screen.minimumRefreshInterval;
    display_link->max_interval_ = display_link->min_interval_;
    display_link->preferred_interval_ = display_link->min_interval_;

    return display_link;
  }

  return nullptr;
}

base::TimeTicks CADisplayLinkMac::GetCurrentTime() const {
  return base::TimeTicks();
}

CADisplayLinkMac::CADisplayLinkMac(CGDirectDisplayID display_id)
    : display_id_(display_id), objc_state_(std::make_unique<ObjCState>()) {}

CADisplayLinkMac::~CADisplayLinkMac() {
  // We must manually invalidate the CADisplayLink as its addToRunLoop keeps
  // strong reference to its target. Thus, releasing our objc_state won't
  // really result in destroying the object.
  if (@available(macos 14.0, *)) {
    if (objc_state_->display_link) {
      [objc_state_->display_link invalidate];
    }
  }
}

std::unique_ptr<VSyncCallbackMac> CADisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback) {
  // Make CADisplayLink callbacks to run on the same RUNLOOP of the register
  // thread without PostTask accross threads.
  auto new_callback = base::WrapUnique(new VSyncCallbackMac(
      base::BindOnce(&CADisplayLinkMac::UnregisterCallback, this),
      std::move(callback), /*post_callback_to_ctor_thread=*/false));

  vsync_callback_ = new_callback->weak_factory_.GetWeakPtr();

  // Ensure that CADisplayLink is running.
  if (@available(macos 14.0, *)) {
    objc_state_->display_link.paused = NO;
  }

  return new_callback;
}

void CADisplayLinkMac::UnregisterCallback(VSyncCallbackMac* callback) {
  vsync_callback_ = nullptr;
}

}  // namespace ui
