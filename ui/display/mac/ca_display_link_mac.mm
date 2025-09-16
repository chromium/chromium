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
  NSScreen* screen = GetNSScreenFromDisplayID(display_id_);
  return 1.0 / screen.minimumRefreshInterval;
}

void CADisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  NSScreen* screen = GetNSScreenFromDisplayID(display_id_);
  min_interval = base::Seconds(1) * screen.minimumRefreshInterval;
  // No support for dynamic refresh range for now. Just return the minimum
  // interval instead of using screen.maximumRefreshInterval and
  // screen.displayUpdateGranularity.
  max_interval = min_interval;
  granularity = min_interval;
}

// static
scoped_refptr<DisplayLinkMac> CADisplayLinkMac::GetForDisplayOnCurrentThread(
    CGDirectDisplayID display_id) {
  if (@available(macos 14.0, *)) {
    scoped_refptr<CADisplayLinkMac> display_link(
        new CADisplayLinkMac(display_id));
    auto* objc_state = display_link->objc_state_.get();

    NSScreen* screen = GetNSScreenFromDisplayID(display_id);
    if (!screen) {
      return nullptr;
    }

    objc_state->target = [[CADisplayLinkTarget alloc] init];
    objc_state->display_link = [screen displayLinkWithTarget:objc_state->target
                                                    selector:@selector(step:)];

    if (!objc_state->display_link) {
      return nullptr;
    }

    // Pause CADisplaylink callback until a request for start.
    objc_state->display_link.paused = YES;

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
        base::Seconds(1) * screen.minimumRefreshInterval;
    display_link->max_interval_ = display_link->min_interval_;
    display_link->preferred_interval_ = display_link->min_interval_;

    return display_link;
  }

  return nullptr;
}

base::TimeTicks CADisplayLinkMac::GetCurrentTime() const {
  return base::TimeTicks() + base::Seconds(CACurrentMediaTime());
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
