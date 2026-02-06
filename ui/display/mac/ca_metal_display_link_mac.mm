// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/ca_metal_display_link_mac.h"

#import <AppKit/AppKit.h>
#include <Metal/Metal.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/mac/screen_utils_mac.h"

API_AVAILABLE(macos(14.0))
@interface DisplayLinkDelegate : NSObject <CAMetalDisplayLinkDelegate>
- (instancetype)initWithLayer:(CAMetalLayer*)layer;
- (void)metalDisplayLink:(CAMetalDisplayLink*)link
             needsUpdate:(CAMetalDisplayLinkUpdate*)update;
- (void)setCallback:
    (base::RepeatingCallback<void(CAMetalDisplayLink*,
                                  CAMetalDisplayLinkUpdate*)>)callback;
- (void)setPresentationCallback:
    (base::RepeatingCallback<void(id<MTLDrawable>)>)callback;
@end

@implementation DisplayLinkDelegate {
  base::RepeatingCallback<void(CAMetalDisplayLink*, CAMetalDisplayLinkUpdate*)>
      _callback;
  base::RepeatingCallback<void(id<MTLDrawable>)> _presentationCallback;
}

- (instancetype)initWithLayer:(CAMetalLayer*)layer {
  if ((self = [super init])) {
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    if (!layer.device) {
      layer.device = layer.preferredDevice;
    }
  }
  return self;
}

- (void)metalDisplayLink:(CAMetalDisplayLink*)link
             needsUpdate:(CAMetalDisplayLinkUpdate*)update {
  if (!_presentationCallback.is_null()) {
    id<CAMetalDrawable> drawable = [update drawable];

    __weak DisplayLinkDelegate* weakSelf = self;
    [drawable addPresentedHandler:^(id<MTLDrawable> d) {
      // Convert weak reference back to strong to ensure 'self' exists
      // for the duration of this block execution. Also ensure
      // _presentationCallback has not been unregistered.
      __strong DisplayLinkDelegate* strongSelf = weakSelf;
      if (strongSelf && !strongSelf->_presentationCallback.is_null()) {
        strongSelf->_presentationCallback.Run(d);
      }
    }];
  }

  _callback.Run(link, update);
}

- (void)setCallback:
    (base::RepeatingCallback<void(CAMetalDisplayLink*,
                                  CAMetalDisplayLinkUpdate*)>)callback {
  _callback = callback;
}

- (void)setPresentationCallback:
    (base::RepeatingCallback<void(id<MTLDrawable>)>)callback {
  _presentationCallback = callback;
}

@end

namespace ui {

namespace {
// While the interval between consecutive VSync callback
// |update.targetTimestamp| parameters is typically very small (e.g. 2
// microseconds), we use 800 microseconds here. This provides sufficient margin
// to identify the current frame interval while remaining significantly lower
// than the 8333 microsecond interval of a 120Hz display. This fallback is rare
// and intended only for cases where VSync parameters are invalid.
bool AlmostEqual(base::TimeDelta a, base::TimeDelta b) {
  return (a - b).magnitude() < base::Microseconds(800);
}

API_AVAILABLE(macos(14.0))
ui::VSyncParamsMac ComputeVSyncParametersMac(CAMetalDisplayLink* display_link,
                                             CAMetalDisplayLinkUpdate* update,
                                             base::TimeTicks last_target_time,
                                             CGDirectDisplayID display_id) {
  // |callback_time| is the time the callback function is called - current time.
  base::TimeTicks callback_time =
      base::TimeTicks() + base::Seconds(CACurrentMediaTime());

  // Calculate the frame interval.
  base::TimeTicks target_time =
      base::TimeTicks() + base::Seconds(update.targetTimestamp);
  base::TimeDelta interval = target_time - last_target_time;
  if (target_time.is_null() || last_target_time.is_null() ||
      !interval.is_positive()) {
    interval = display::GetNSScreenRefreshInterval(display_id);
  }

  // The time the system estimates until the display of the next frame.
  base::TimeTicks target_presented_time =
      base::TimeTicks() + base::Seconds(update.targetPresentationTimestamp);

  if (target_presented_time.is_null()) {
    // Just assign a value as we observe 5 frames away @ 120Hz and 2 frames
    // aways @ 60Hz in our tests.
    if (AlmostEqual(interval, base::Hertz(120))) {
      target_presented_time = callback_time + interval * 5;
    } else {
      target_presented_time = callback_time + interval * 2;
    }
  }

  ui::VSyncParamsMac params;
  params.callback_times_valid = true;
  params.callback_timebase = callback_time;
  params.callback_interval = interval;

  params.display_times_valid = true;
  params.display_timebase = target_presented_time;
  params.display_interval = interval;

  params.drawable_id = update.drawable.drawableID;

  return params;
}
}  // namespace

struct MetalObjCState {
  CAMetalDisplayLink* __strong display_link API_AVAILABLE(macos(14.0));
  DisplayLinkDelegate* __strong delegate API_AVAILABLE(macos(14.0));
};

void CAMetalDisplayLinkMac::MetalDisplayLinkCallback(
    CAMetalDisplayLink* display_link,
    CAMetalDisplayLinkUpdate* update) {
  TRACE_EVENT0("ui", "MetalDisplayLinkCallback");

  if (@available(macos 14.0, *)) {
    if (!vsync_callback_) {
      return;
    }

    ui::VSyncParamsMac params = ComputeVSyncParametersMac(
        display_link, update, last_target_time_, display_id_);

    // UnregisterCallback() might be called while running the callbacks.
    vsync_callback_->callback_for_displaylink_thread_.Run(params);

    last_target_time_ =
        base::TimeTicks() + base::Seconds(update.targetTimestamp);
    last_target_time_is_valid_ = true;
  }
}

void CAMetalDisplayLinkMac::MetalPresentationCallback(
    id<MTLDrawable> drawable) {
  if (!presented_callback_) {
    return;
  }

  base::TimeTicks presented_time =
      base::TimeTicks() + base::Seconds(drawable.presentedTime);
  presented_callback_->callback_for_displaylink_thread_.Run(drawable.drawableID,
                                                            presented_time);
}

base::TimeDelta CAMetalDisplayLinkMac::GetRefreshInterval() const {
  return display::GetNSScreenRefreshInterval(display_id_);
}

void CAMetalDisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  display::GetNSScreenRefreshIntervalRange(display_id_, min_interval,
                                           max_interval, granularity);
}

// static
scoped_refptr<DisplayLinkMac> CAMetalDisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id) {
  NOTREACHED();
}

// static
scoped_refptr<DisplayLinkMac> CAMetalDisplayLinkMac::MetalGetForDisplay(
    CGDirectDisplayID display_id,
    CAMetalLayer* metal_layer) {
  if (@available(macos 14.0, *)) {
    scoped_refptr<CAMetalDisplayLinkMac> display_link(
        new CAMetalDisplayLinkMac(display_id));
    auto* objc_state = display_link->objc_state_.get();

    objc_state->delegate =
        [[DisplayLinkDelegate alloc] initWithLayer:metal_layer];
    if (!objc_state->delegate) {
      LOG(ERROR) << "CAMetalDisplayLinkMac::MetalGetForDisplay(). Fail to "
                    "alloc DisplayLinkDelegate";
    }

    objc_state->display_link =
        [[CAMetalDisplayLink alloc] initWithMetalLayer:metal_layer];
    objc_state->display_link.delegate = objc_state->delegate;
    objc_state->display_link.preferredFrameLatency = 1.0f;

    if (!objc_state->display_link) {
      RecordDisplayLinkCreation(false);
      return nullptr;
    }

    RecordDisplayLinkCreation(true);

    // Pause CADisplaylink callback until a request for start.
    objc_state->display_link.paused = YES;
    [objc_state->display_link addToRunLoop:NSRunLoop.currentRunLoop
                                   forMode:NSRunLoopCommonModes];

    // Set the DisplayLinkDeletate's callback to call into the C++ code.
    [objc_state->delegate
        setCallback:base::BindRepeating(
                        &CAMetalDisplayLinkMac::MetalDisplayLinkCallback,
                        display_link->weak_factory_.GetWeakPtr())];
    return display_link;
  }

  return nullptr;
}

base::TimeTicks CAMetalDisplayLinkMac::GetCurrentTime() const {
  return base::TimeTicks() + base::Seconds(CACurrentMediaTime());
}

CAMetalDisplayLinkMac::CAMetalDisplayLinkMac(CGDirectDisplayID display_id)
    : display_id_(display_id),
      objc_state_(std::make_unique<MetalObjCState>()) {}

CAMetalDisplayLinkMac::~CAMetalDisplayLinkMac() {
  // We must manually invalidate the CADisplayLink as its addToRunLoop keeps
  // strong reference to its target. Thus, releasing our objc_state won't
  // really result in destroying the object.
  if (@available(macos 14.0, *)) {
    if (objc_state_->display_link) {
      [objc_state_->display_link invalidate];
    }
  }
}

std::unique_ptr<VSyncCallbackMac> CAMetalDisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback) {
  // Make CADisplayLink callbacks to run on the same RUNLOOP of the register
  // thread without PostTask accross threads.
  auto new_callback = base::WrapUnique(new VSyncCallbackMac(
      base::BindOnce(&CAMetalDisplayLinkMac::UnregisterCallback, this),
      std::move(callback), /*post_callback_to_ctor_thread=*/false));

  vsync_callback_ = new_callback->weak_factory_.GetWeakPtr();

  // Ensure that CADisplayLink is running.
  if (@available(macos 14.0, *)) {
    objc_state_->display_link.paused = NO;
  }

  return new_callback;
}

void CAMetalDisplayLinkMac::UnregisterCallback(VSyncCallbackMac* callback) {
  vsync_callback_ = nullptr;
  if (@available(macos 14.0, *)) {
    objc_state_->display_link.paused = YES;
  }

  last_target_time_is_valid_ = false;
  last_target_time_ = base::TimeTicks();
}

std::unique_ptr<PresentationCallbackMac>
CAMetalDisplayLinkMac::RegisterPresentationCallback(
    PresentationCallbackMac::Callback callback) {
  [objc_state_->delegate
      setPresentationCallback:
          base::BindRepeating(&CAMetalDisplayLinkMac::MetalPresentationCallback,
                              weak_factory_.GetWeakPtr())];

  // Make CADisplayLink callbacks to run on the same RUNLOOP of the register
  // thread without PostTask accross threads.
  auto new_callback = base::WrapUnique(new PresentationCallbackMac(
      base::BindOnce(&CAMetalDisplayLinkMac::UnregisterPresentationCallback,
                     this),
      std::move(callback), /*post_callback_to_ctor_thread=*/false));

  presented_callback_ = new_callback->weak_factory_.GetWeakPtr();

  return new_callback;
}

void CAMetalDisplayLinkMac::UnregisterPresentationCallback(
    PresentationCallbackMac* callback) {
  [objc_state_->delegate
      setPresentationCallback:base::RepeatingCallback<void(id<MTLDrawable>)>()];
  presented_callback_ = nullptr;
}

}  // namespace ui
