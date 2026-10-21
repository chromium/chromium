// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/ca_display_link_mac.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <mach/mach.h>
#import <mach/message.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/flat_set.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/display/mac/screen_utils_mac.h"

extern "C" {
// Undocumented CoreGraphics framework functions.
using CGSConnectionID = uint32_t;
CGSConnectionID CGSMainConnectionID();
CGError CGSGetEventPort(CGSConnectionID connection_id, mach_port_t* port);
CGEventRef CGEventCreateNextEvent(CGSConnectionID connection_id);

// Undocumented SkyLight framework functions.
// SLSGetDisplayLink() is a private function (SPI) that creates a CADisplayLink
// directly from a CGDirectDisplayID without requiring NSApplication or AppKit
// objects (e.g., NSScreen, NSWindow, or NSView).
//
// `framework_dirs = [ "$mac_sdk_path/System/Library/PrivateFrameworks" ]` is
// added to //ui/display/BUILD.gn to link SkyLight.framework. Remove this
// private framework dependency after Apple provides a public API for this.
CADisplayLink* SLSGetDisplayLink(CGDirectDisplayID display_id,
                                 id target,
                                 SEL action) API_AVAILABLE(macos(14.0));
}

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

  static CADisplayLinkGlobals& Get() {
    static base::NoDestructor<CADisplayLinkGlobals> instance;
    return *instance;
  }

  // Returns false once the WindowServerEventPumpThread has failed, after which
  // CADisplayLinkMac can no longer rely on receiving display reconfiguration
  // and wake events.
  bool IsEventPumpReady() {
    base::AutoLock auto_lock(lock);
    return !event_pump_failed;
  }

  // Marks the WindowServerEventPumpThread as failed. This is permanent: all
  // subsequent CADisplayLinkMac::GetForDisplay() calls return nullptr so that
  // callers fall back to a timer-based VSync source.
  void SetWindowServerEventPumpFailed() {
    base::AutoLock auto_lock(lock);
    event_pump_failed = true;
  }

  base::Lock lock;

  // Set to true if WindowServerEventPumpThread fails to pump WindowServer
  // events. See IsEventPumpReady().
  bool event_pump_failed GUARDED_BY(lock) = false;

  // Indicates whether the display creation has been logged within the
  // 'Viz.ExternalBeginFrameSourceMac.DisplayLink.Create2' histogram.
  absl::flat_hash_set<CGDirectDisplayID> recorded_displays GUARDED_BY(lock);
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
    interval = display::GetCGRefreshInterval(display_id);
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

void RecordWindowServerEventPumpResult(
    WindowServerEventPumpThread::Result result) {
  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayLink.WindowServerEventPumpResult",
                            result);
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
  return display::GetCGRefreshInterval(display_id_);
}

void CADisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  display::GetNSScreenRefreshIntervalRange(display_id_, min_interval,
                                           max_interval, granularity);
}

// static
// This function is called from both the GPU and the Browser process.
void CADisplayLinkMac::TryRecordDisplayLinkCreation(
    CGDirectDisplayID display_id,
    bool success) {
  auto& globals = CADisplayLinkGlobals::Get();
  base::AutoLock lock(globals.lock);

  auto [it, inserted] = globals.recorded_displays.insert(display_id);
  if (inserted) {
    UMA_HISTOGRAM_BOOLEAN("Viz.DisplayLink.Create.GPU.CADisplayLinkV2",
                          success);
    RecordDisplayLinkCreation(success);
  }
}

// static
scoped_refptr<DisplayLinkMac> CADisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id,
    bool in_gpu_process) {
  if (@available(macos 14.0, *)) {
    TRACE_EVENT("gpu", "CADisplayLinkMac::GetForDisplay");

    // Without a working WindowServer event pump, CADisplayLink may stop firing
    // after display reconfiguration or power resume. Refuse to create one so
    // the caller falls back to a timer.
    if (!CADisplayLinkGlobals::Get().IsEventPumpReady()) {
      TryRecordDisplayLinkCreation(display_id, /*success=*/false);
      return nullptr;
    }

    scoped_refptr<CADisplayLinkMac> display_link(
        new CADisplayLinkMac(display_id));
    auto* objc_state = display_link->objc_state_.get();

    objc_state->target = [[CADisplayLinkTarget alloc] init];
    objc_state->display_link =
        SLSGetDisplayLink(display_id, objc_state->target, @selector(step:));

    if (!objc_state->display_link) {
      TryRecordDisplayLinkCreation(display_id, /*success=*/false);
      return nullptr;
    }

    TryRecordDisplayLinkCreation(display_id, /*success=*/true);

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

// static
bool WindowServerEventPumpThread::NSAppInitialized() {
  return NSApp != nil;
}

// FrameSinkManagerImpl creates and owns WindowServerEventPumpThread when
// the CADisplayLinkInGpu feature is enabled.
WindowServerEventPumpThread::WindowServerEventPumpThread(
    base::RepeatingCallback<void(int64_t)> update_vsync_displays_cb)
    : base::Thread("WindowServerEventPumpThread"),
      update_vsync_displays_cb_(std::move(update_vsync_displays_cb)),
      viz_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  weak_ptr_ = weak_factory_.GetWeakPtr();

  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::NS_RUNLOOP;
  thread_options.thread_type = base::ThreadType::kPresentation;
  StartWithOptions(std::move(thread_options));

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowServerEventPumpThread::PumpWindowServerEvents,
                     base::Unretained(this)));
}

// In a standard out-of-process GPU scenario,
// FrameSinkManagerImpl::~FrameSinkManagerImpl() is typically not called during
// browser shutdown because Chromium child processes (such as the GPU and
// renderer processes) use a "fast shutdown" mechanism and terminate
// immediately. In an in-process GPU scenario, this thread is not needed
// because the GPU code runs directly in the main process.
// As a result, ~WindowServerEventPumpThread() is only invoked during unit
// tests.
WindowServerEventPumpThread::~WindowServerEventPumpThread() {
  if (shutdown_) {
    return;
  }

  shutdown_ = true;

  // Send an empty message to wake up the thread and quit the while loop.
  // See WindowServerEventPumpThread::PumpWindowServerEvents() for details.
  CGSConnectionID connection_id = CGSMainConnectionID();
  mach_port_t port = MACH_PORT_NULL;
  CGError err = CGSGetEventPort(connection_id, &port);

  if (err == kCGErrorSuccess) {
    mach_msg_empty_send_t msg = {};
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0);
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_id = 0;

    // Sent with a timeout up to 5 seconds if the queue is full.
    mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg),
             /*rcv_size=*/0, /*rcv_name=*/MACH_PORT_NULL, /*timeout=*/5000,
             /*notify=*/MACH_PORT_NULL);
  } else {
    LOG(ERROR)
        << "CGSGetEventPort() in ~WindowServerEventPumpThread failed. Error: "
        << err;
  }

  Stop();
}

void WindowServerEventPumpThread::PumpWindowServerEvents() {
  CGSConnectionID connection_id = CGSMainConnectionID();
  // We haven't seen this happen in practice, but WebKit's implementation
  // checks for 0 as an invalid connection ID. See
  // WebKit/Source/WebKitLegacy/mac/WebView/WebWindowAnimation.mm.
  if (connection_id == 0) {
    DLOG(WARNING) << "Failed to get main CGS connection ID for CADisplayLink";
    DisableDisplayLink(Result::kInvalidMainConnection);
    return;
  }

  // Undocumented WindowServer connection port. Display reconfig/wake
  // messages are sent to this port so CADisplayLink can receive them.
  // Normally, NSApplication pumps the event loop on a client's behalf:
  // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/EventArchitecture/EventArchitecture.html#//apple_ref/doc/uid/10000060i-CH3-SW4
  mach_port_t port = MACH_PORT_NULL;
  CGError err = CGSGetEventPort(connection_id, &port);
  if (err != kCGErrorSuccess || port == MACH_PORT_NULL) {
    DLOG(WARNING)
        << "CGSGetEventPort() in WindowServerEventPump failed. Error: " << err;
    DisableDisplayLink(Result::kGetEventPortFailed);
    return;
  }

  // Local copies for crash dumps. One base::debug::Alias() each is enough:
  // once aliased, stores are flushed before every opaque call below.
  // TODO (crbug.com/574076803): Remove the debug code when they are no longer
  // needed.
  uint64_t wakeups = 0;
  uint64_t total_events_pulled = 0;
  mach_msg_return_t last_mr = MACH_MSG_SUCCESS;
  mach_msg_header_t last_header = {};
  uint32_t events_pulled_this_wakeup = 0;
  CGEventType last_event_type = kCGEventNull;
  base::debug::Alias(&wakeups);
  base::debug::Alias(&total_events_pulled);
  base::debug::Alias(&last_mr);
  base::debug::Alias(&last_header);
  base::debug::Alias(&events_pulled_this_wakeup);
  base::debug::Alias(&last_event_type);

  // Success is recorded only once, after the first drain of the event queue.
  bool success_recorded = false;
  while (!shutdown_) {
    // Wait for signal. The thread pauses until a message arrives on the
    // specific mach port.
    struct ReceiveMessage {
      mach_msg_header_t header;
      mach_msg_max_trailer_t trailer;
    };
    ReceiveMessage message = {};
    constexpr mach_msg_size_t kReceiveSize = sizeof(message);
    mach_msg_return_t mr = mach_msg(&message.header, MACH_RCV_MSG, 0,
                                    kReceiveSize, port, 0, MACH_PORT_NULL);

    // For crash dump analysis only.
    ++wakeups;
    last_mr = mr;
    last_header = message.header;

    if (mr != MACH_RCV_TOO_LARGE && mr != MACH_MSG_SUCCESS) {
      DLOG(ERROR) << "mach_msg() in WindowServerEventPump failed. Error: "
                  << mr;
      DisableDisplayLink(Result::kMachMsgReceiveFailed);
      return;
    }

    // Observed mach_msg_header_t on success. Every message received so far in
    // local testing on macOS 14+ has had the same values, except for
    // msgh_local_port, which differs between launches:
    //   msgh_bits=0x1100 (remote=0 local=17 voucher=0 complex=0)
    //   msgh_size=24
    //   msgh_remote_port=0x0
    //   msgh_local_port=0xab03, 0x980f, 0x9103, ...
    //   msgh_voucher_port=0x0
    //   msgh_id=0
    // Interpretation:
    //   - remote=0, msgh_remote_port=0: no reply port right was transferred
    //     with the message, so there is no send or send-once right for us to
    //     leak or dispose of.
    //   - local=17 (MACH_MSG_TYPE_PORT_SEND), msgh_local_port: the name of the
    //     WindowServer connection port we are listening on, i.e. `port` above.
    //     It is constant for the lifetime of a process but differs between
    //     launches, as expected for a Mach port name, which is only meaningful
    //     within the process that holds the right.
    //   - voucher=0, msgh_voucher_port=0: no voucher right attached.
    //   - complex=0: no out-of-line memory or port descriptors in the body.
    //   - msgh_size=24 == sizeof(mach_msg_header_t): the message is a bare
    //     header with no payload; it is only a wake-up signal.
    //   - msgh_id=0: WindowServer does not use the ID to distinguish messages.
    // In short, the message carries nothing that mach_msg_destroy() would need
    // to release. MACH_RCV_TOO_LARGE and messages with any other header values
    // have not been observed in local testing, but since the port is not ours,
    // they cannot be ruled out in the field.

    // Record the time it takes to drain the event queue.
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Viz.DisplayLink.DrainEventQueueUs");

    // Pulling events until the queue is empty - dequeuing is what actually
    // dispatches the messages (such as display notifications).
    //
    // Observed events in local testing on macOS 14+ (sample):
    //   type=14 timestamp=52836175789125 flags=0x0
    //   location=(43.5781, 219.793) description=<CGEvent 0x97b7e5ec0>
    // All events pulled in the same drain had the same type and flags.
    // Interpretation:
    //   - type=14 is not a public CGEventType value. It matches NX_SYSDEFINED
    //     (== NSEventTypeSystemDefined), the category WindowServer uses for
    //     system notifications such as display reconfiguration and power
    //     events, i.e. exactly the events this pump exists to flush. It is not
    //     a keyboard or mouse input event (public CGEventType values are 1-7,
    //     10-12 and 22-27), so we are not stealing input meant for another
    //     consumer.
    //   - flags=0x0: no modifier flags, as expected for a non-input event.
    //   - location: the cursor position at the time the event was generated;
    //     not meaningful for a system-defined event.
    //   - description is the bare CFType description and carries no detail.
    // The NX_SYSDEFINED subtype (and data1/data2), which identify the specific
    // system event, are not exposed through public CGEvent getters; they would
    // require -[NSEvent eventWithCGEvent:] to inspect.
    events_pulled_this_wakeup = 0;
    // CGEventCreateNextEvent() follows the Create rule, so ScopedCFTypeRef
    // assumes ownership of the returned +1 reference and releases it on scope
    // exit.
    while (base::apple::ScopedCFTypeRef<CGEventRef> event =
               base::apple::ScopedCFTypeRef<CGEventRef>(
                   CGEventCreateNextEvent(connection_id))) {
      CGEventType event_type = CGEventGetType(event.get());

      // For crash dump analysis only.
      last_event_type = event_type;
      ++events_pulled_this_wakeup;
      ++total_events_pulled;

      // CGEventCreateNextEvent() returns kCGEventNull when the queue is
      // drained.
      if (event_type == kCGEventNull) {
        break;
      }
    }

    if (!success_recorded) {
      success_recorded = true;
      RecordWindowServerEventPumpResult(Result::kSuccess);
    }
  }
}

void WindowServerEventPumpThread::DisableDisplayLink(Result failure) {
  // This may be called from the pump thread. Hop to the Viz thread, which owns
  // FrameSinkManagerImpl and the root frame sinks.
  if (!viz_task_runner_->BelongsToCurrentThread()) {
    viz_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WindowServerEventPumpThread::DisableDisplayLink,
                       weak_ptr_, failure));
    return;
  }

  DCHECK_NE(failure, Result::kSuccess);
  RecordWindowServerEventPumpResult(failure);

  shutdown_ = true;
  Stop();

  // Make CADisplayLinkMac::GetForDisplay() fail all future creation requests.
  CADisplayLinkGlobals::Get().SetWindowServerEventPumpFailed();

  // Force every root frame sink to recreate its DisplayLinkMac. Since
  // CADisplayLinkMac creation now fails, ExternalBeginFrameSourceMac and
  // ImageTransportSurfaceOverlayMacEGL fall back to a timer. The display ID
  // is ignored when the GPU process owns the display link, so pass 0 to
  // update all displays.
  update_vsync_displays_cb_.Run(0);
}

}  // namespace ui
