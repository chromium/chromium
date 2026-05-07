// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/ca_display_link_mac.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CADisplayLink.h>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/display/mac/screen_utils_mac.h"

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
  CADisplayLinkGlobals() = default;
  base::Lock lock;
  // Set of display IDs where CADisplayLink has become unreliable in the GPU
  // process (e.g., due to a power event or system refresh rate change).
  absl::flat_hash_set<int64_t> invalidated_displays GUARDED_BY(lock);

  // Indicate whether the display creation has been logged within the
  // 'Viz.ExternalBeginFrameSourceMac.DisplayLink.Create2' histogram.
  absl::flat_hash_set<CGDirectDisplayID> recorded_displays GUARDED_BY(lock);

  static CADisplayLinkGlobals& Get() {
    static base::NoDestructor<CADisplayLinkGlobals> instance;
    return *instance;
  }
};

API_AVAILABLE(macos(14.0))
ui::VSyncParamsMac ComputeVSyncParametersMac(CADisplayLink* display_link,
                                             CGDirectDisplayID display_id) {
  // The time interval that represents when the last frame displayed.
  base::TimeTicks callback_time =
      base::TimeTicks() + base::Seconds(display_link.timestamp);
  // The time interval that represents when the next frame displays.
  base::TimeTicks target_time =
      base::TimeTicks() + base::Seconds(display_link.targetTimestamp);

  base::TimeDelta interval = base::Seconds(1) * display_link.duration;

  // Sanity check. Inputs should always be valid. Use the default values if this
  // is not the case.
  if (!interval.is_positive()) {
    interval = display::GetNSScreenRefreshInterval(display_id);
  }
  if (callback_time.is_null() || target_time.is_null()) {
    callback_time = base::TimeTicks() + base::Seconds(CACurrentMediaTime());
    target_time = callback_time + interval;
  }

  ui::VSyncParamsMac params;
  params.callback_times_valid = true;
  params.callback_timebase = callback_time;
  params.callback_interval = interval;

  params.display_times_valid = true;
  params.display_timebase = target_time;
  params.display_interval = interval;

  return params;
}
}  // namespace

struct ObjCState {
  CADisplayLink* __strong display_link API_AVAILABLE(macos(14.0));
  CADisplayLinkTarget* __strong target API_AVAILABLE(macos(14.0));
};

void CADisplayLinkMac::Step() {
  TRACE_EVENT0("gpu", "CADisplayLinkCallback");

  if (@available(macos 14.0, *)) {
    if (!vsync_callback_) {
      return;
    }

    ui::VSyncParamsMac params =
        ComputeVSyncParametersMac(objc_state_->display_link, display_id_);

    // UnregisterCallback() might be called while running the callbacks.
    vsync_callback_->callback_for_displaylink_thread_.Run(params);
  }
}

base::TimeDelta CADisplayLinkMac::GetRefreshInterval() const {
  return display::GetNSScreenRefreshInterval(display_id_);
}

void CADisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  display::GetNSScreenRefreshIntervalRange(display_id_, min_interval,
                                           max_interval, granularity);
}

// static
void CADisplayLinkMac::TryRecordDisplayLinkCreation(
    CGDirectDisplayID display_id,
    bool success,
    bool in_gpu_process) {
  // Only record CADisplayLink creation status from one process to avoid
  // duplicate logging as they cannot be tracked across multiple processes using
  // a single |recorded_displays| set. If kCADisplayLinkInGpuThenInBrowser
  // (disabled by default) is enabled, it should record the ones in_gpu_process
  // instead.
  if (in_gpu_process) {
    return;
  }
  auto& globals = CADisplayLinkGlobals::Get();
  base::AutoLock lock(globals.lock);

  auto [it, inserted] = globals.recorded_displays.insert(display_id);
  if (inserted) {
    RecordDisplayLinkCreation(success);
  }
}

// static
scoped_refptr<DisplayLinkMac> CADisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id,
    bool in_gpu_process) {
  if (@available(macos 14.0, *)) {
    scoped_refptr<CADisplayLinkMac> display_link(
        new CADisplayLinkMac(display_id));
    auto* objc_state = display_link->objc_state_.get();

    NSScreen* screen = display::GetNSScreenFromDisplayID(display_id);
    if (!screen) {
      TryRecordDisplayLinkCreation(display_id, /*success=*/false,
                                   in_gpu_process);
      return nullptr;
    }

    objc_state->target = [[CADisplayLinkTarget alloc] init];
    objc_state->display_link = [screen displayLinkWithTarget:objc_state->target
                                                    selector:@selector(step:)];

    if (!objc_state->display_link) {
      TryRecordDisplayLinkCreation(display_id, /*success=*/false,
                                   in_gpu_process);
      return nullptr;
    }

    TryRecordDisplayLinkCreation(display_id, /*success=*/true, in_gpu_process);

    // Pause CADisplaylink callback until a request for start.
    objc_state->display_link.paused = YES;

    // This display link interface requires the task executor of the current
    // thread (CrGpuMain or VizCompositorThread) to run with
    // MessagePumpType::NS_RUNLOOP. CADisplayLinkTarget and display_link are
    // NSObject, and MessagePumpType::DEFAULT does not support system work.
    // There will be no callbacks (CADisplayLinkTarget::step()) at all if
    // MessagePumpType NS_RUNLOOP is not chosen during thread initialization.
    [objc_state->display_link addToRunLoop:NSRunLoop.currentRunLoop
                                   forMode:NSRunLoopCommonModes];

    // Set the CADisplayLinkTarget's callback to call back into the C++ code.
    [objc_state->target
        setCallback:base::BindRepeating(
                        &CADisplayLinkMac::Step,
                        display_link->weak_factory_.GetWeakPtr())];

    TRACE_EVENT("gpu", "CADisplayLinkMac::GetForDisplay succeeded");

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
  TRACE_EVENT("gpu", "CADisplayLinkMac::RegisterCallback");
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
  TRACE_EVENT("gpu", "CADisplayLinkMac::UnregisterCallback");
  vsync_callback_ = nullptr;
  if (@available(macos 14.0, *)) {
    objc_state_->display_link.paused = YES;
  }
}

bool CADisplayLinkMac::NotifyEventAndCheckValidity(int64_t display_id) {
  base::AutoLock lock(CADisplayLinkGlobals::Get().lock);
  CADisplayLinkGlobals::Get().invalidated_displays.insert(display_id);
  return false;
}

// static
bool CADisplayLinkMac::IsValidInGpuProcess(int64_t display_id) {
  base::AutoLock lock(CADisplayLinkGlobals::Get().lock);
  auto& invalidated_displays = CADisplayLinkGlobals::Get().invalidated_displays;
  return invalidated_displays.find(display_id) == invalidated_displays.end();
}

}  // namespace ui
