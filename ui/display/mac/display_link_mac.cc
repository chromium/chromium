// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/display_link_mac.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"

namespace base {

template <>
struct ScopedTypeRefTraits<CVDisplayLinkRef> {
  static CVDisplayLinkRef InvalidValue() { return nullptr; }
  static CVDisplayLinkRef Retain(CVDisplayLinkRef object) {
    return CVDisplayLinkRetain(object);
  }
  static void Release(CVDisplayLinkRef object) { CVDisplayLinkRelease(object); }
};

}  // namespace base

namespace ui {

using DisplayLinkMap = std::map<CGDirectDisplayID, DisplayLinkMac*>;

namespace {

bool ComputeVSyncParameters(const CVTimeStamp& cv_time,
                            base::TimeTicks* timebase,
                            base::TimeDelta* interval) {
  // Verify that videoRefreshPeriod fits in 32 bits.
  DCHECK((cv_time.videoRefreshPeriod & ~0xFFFF'FFFFull) == 0ull);

  // Verify that the numerator and denominator make some sense.
  uint32_t numerator = static_cast<uint32_t>(cv_time.videoRefreshPeriod);
  uint32_t denominator = cv_time.videoTimeScale;
  if (numerator == 0 || denominator == 0) {
    LOG(WARNING) << "Unexpected numerator or denominator, bailing.";
    return false;
  }

  base::CheckedNumeric<int64_t> interval_us(base::Time::kMicrosecondsPerSecond);
  interval_us *= numerator;
  interval_us /= denominator;
  if (!interval_us.IsValid()) {
    LOG(DFATAL) << "Bailing due to overflow: "
                << base::Time::kMicrosecondsPerSecond << " * " << numerator
                << " / " << denominator;
    return false;
  }

  *timebase = base::TimeTicks::FromMachAbsoluteTime(cv_time.hostTime);
  *interval = base::Microseconds(int64_t{interval_us.ValueOrDie()});
  return true;
}

struct DisplayLinkGlobals {
  // |map| maybe accessed on anythread but only modified on the main thread..
  std::map<CGDirectDisplayID, DisplayLinkMac*> GUARDED_BY(lock) map;
  // Making any calls to the CVDisplayLink API while `lock` is held can
  // result in deadlock, because `lock` is taken inside the CVDisplayLink
  // system callback.
  // https://crbug.com/1427235#c2
  base::Lock lock;

  static DisplayLinkGlobals& Get() {
    static base::NoDestructor<DisplayLinkGlobals> instance;
    return *instance;
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplayLinkMac

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id) {
#if DCHECK_IS_ON()
  // This function must be always called on the same thread. Check the thread
  // id instead of the task runner. The task runner might not be available in
  // the test.
  static const base::PlatformThreadId first_thread_id =
      base::PlatformThread::CurrentId();
  base::PlatformThreadId current_thread_id = base::PlatformThread::CurrentId();

  DCHECK(current_thread_id == first_thread_id);
#endif

  if (!display_id) {
    return nullptr;
  }

  // Return the existing display link for this display, if it exists.
  auto& globals = DisplayLinkGlobals::Get();
  {
    base::AutoLock lock(globals.lock);
    auto found = globals.map.find(display_id);
    if (found != globals.map.end()) {
      return found->second;
    }
  }

  // Create a new DisplayLink, outside of the lock.
  CVReturn ret = kCVReturnSuccess;

  // It's safe to call CVDisplayLinkCreateWithCGDisplay,
  // CVDisplayLinkGetCurrentCGDisplay, and CVDisplayLinkSetOutputCallback
  // without holding a lock to globals.map because DisplayLinkMac is always
  // created/destroyed on the same thread. Holding a lock here can result in
  // deadlock.

  base::ScopedTypeRef<CVDisplayLinkRef> display_link;
  ret = CVDisplayLinkCreateWithCGDisplay(display_id,
                                         display_link.InitializeInto());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkCreateWithCGDisplay failed. CVReturn: " << ret;
    return nullptr;
  }

  // Workaround for bug https://crbug.com/1218720. According to
  // https://hg.mozilla.org/releases/mozilla-esr68/rev/db0628eadb86,
  // CVDisplayLinkCreateWithCGDisplays()
  // (called by CVDisplayLinkCreateWithCGDisplay()) sometimes
  // creates a CVDisplayLinkRef with an uninitialized (nulled) internal
  // pointer. If we continue to use this CVDisplayLinkRef, we will
  // eventually crash in CVCGDisplayLink::getDisplayTimes(), where the
  // internal pointer is dereferenced. Fortunately, when this happens
  // another internal variable is also left uninitialized (zeroed),
  // which is accessible via CVDisplayLinkGetCurrentCGDisplay(). In
  // normal conditions the current display is never zero.
  if ((ret == kCVReturnSuccess) &&
      (CVDisplayLinkGetCurrentCGDisplay(display_link) == 0)) {
    LOG(ERROR)
        << "CVDisplayLinkCreateWithCGDisplay failed (no current display)";
    return nullptr;
  }

  ret = CVDisplayLinkSetOutputCallback(display_link, &DisplayLinkCallback,
                                       reinterpret_cast<void*>(display_id));
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkSetOutputCallback failed. CVReturn: " << ret;
    return nullptr;
  }

  scoped_refptr<DisplayLinkMac> result(
      new DisplayLinkMac(display_id, display_link));
  {
    base::AutoLock lock(globals.lock);
    globals.map.emplace(display_id, result.get());
  }
  return result;
}

double DisplayLinkMac::GetRefreshRate() {
  double refresh_rate = 0;
  CVTime cv_time =
      CVDisplayLinkGetNominalOutputVideoRefreshPeriod(display_link_);
  if (!(cv_time.flags & kCVTimeIsIndefinite))
    refresh_rate = (static_cast<double>(cv_time.timeScale) /
                    static_cast<double>(cv_time.timeValue));

  return refresh_rate;
}

base::TimeTicks DisplayLinkMac::GetCurrentTime() {
  CVTimeStamp out_time;
  CVReturn ret = CVDisplayLinkGetCurrentTime(display_link_, &out_time);

  if (ret == kCVReturnSuccess) {
    return base::TimeTicks::FromMachAbsoluteTime(out_time.hostTime);
  } else {
    return base::TimeTicks();
  }
}

DisplayLinkMac::DisplayLinkMac(
    CGDirectDisplayID display_id,
    base::ScopedTypeRef<CVDisplayLinkRef> display_link)
    : display_id_(display_id), display_link_(display_link) {}

DisplayLinkMac::~DisplayLinkMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(display_link_mac_sequence_checker_);

  auto& globals = DisplayLinkGlobals::Get();
  base::AutoLock lock(globals.lock);

  auto found = globals.map.find(display_id_);
  DCHECK(found != globals.map.end());
  DCHECK(found->second == this);
  globals.map.erase(found);
}

// static
CVReturn DisplayLinkMac::DisplayLinkCallback(CVDisplayLinkRef display_link,
                                             const CVTimeStamp* now,
                                             const CVTimeStamp* output_time,
                                             CVOptionFlags flags_in,
                                             CVOptionFlags* flags_out,
                                             void* context) {
  TRACE_EVENT0("ui", "DisplayLinkMac::DisplayLinkCallback");
  // This function is called on the system CVDisplayLink thread.

  // Convert the time parameters to our VSync parameters.
  VSyncParamsMac params;
  params.callback_times_valid = ComputeVSyncParameters(
      *now, &params.callback_timebase, &params.callback_interval);
  params.display_times_valid = ComputeVSyncParameters(
      *output_time, &params.display_timebase, &params.display_interval);

  // Locate the DisplayLinkMac for this display.
  auto& globals = DisplayLinkGlobals::Get();
  base::AutoLock lock(globals.lock);

  CGDirectDisplayID display_id =
      static_cast<CGDirectDisplayID>(reinterpret_cast<uintptr_t>(context));
  auto found = globals.map.find(display_id);
  if (found == globals.map.end()) {
    return kCVReturnSuccess;
  }

  // Issue all of its callbacks.
  DisplayLinkMac* display_link_mac = found->second;
  for (auto* callback : display_link_mac->callbacks_) {
    callback->callback_for_cvdisplaylink_thread_.Run(params);
  }

  return kCVReturnSuccess;
}

std::unique_ptr<VSyncCallbackMac> DisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback,
    bool do_callback_on_register_thread) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(display_link_mac_sequence_checker_);
  auto& globals = DisplayLinkGlobals::Get();

  // Make sure the callback is added to |callbacks_| before calling
  // CVDisplayLinkStart.
  std::unique_ptr<VSyncCallbackMac> new_observer(new VSyncCallbackMac(
      this, std::move(callback), do_callback_on_register_thread));
  bool need_to_start_display_link = false;
  std::pair<std::set<VSyncCallbackMac*>::iterator, bool> insert_result;
  {
    base::AutoLock lock(globals.lock);
    need_to_start_display_link = callbacks_.empty();
    insert_result = callbacks_.insert(new_observer.get());
  }

  // Start the display link, if needed. If we fail to start the link, return
  // nullptr.
  if (need_to_start_display_link) {
    DCHECK(!CVDisplayLinkIsRunning(display_link_));

    // It's safe to call CVDisplayLinkStart and CVDisplayLinkStop without
    // holding a lock to globals.lock because |callbacks_| are always modified
    // on the same ctor thread, and also CVDisplayLinkStart and
    // CVDisplayLinkStop are always called on the same thread. Holding a lock
    // here can result in deadlock inside CVDisplayLinkStart.
    CVReturn ret = CVDisplayLinkStart(display_link_);
    if (ret != kCVReturnSuccess) {
      LOG(ERROR) << "CVDisplayLinkStart failed. CVReturn: " << ret;

      base::AutoLock lock(globals.lock);
      callbacks_.erase(insert_result.first);
      return nullptr;
    }
  }

  return new_observer;
}

void DisplayLinkMac::UnregisterCallback(VSyncCallbackMac* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(display_link_mac_sequence_checker_);

  bool need_to_stop_display_link = false;
  auto found = callbacks_.find(observer);
  CHECK(found != callbacks_.end());
  {
    auto& globals = DisplayLinkGlobals::Get();
    base::AutoLock lock(globals.lock);
    callbacks_.erase(found);
    need_to_stop_display_link = callbacks_.empty();
  }

  // Stop the CVDisplayLink if all observers are removed.
  if (need_to_stop_display_link) {
    DCHECK(CVDisplayLinkIsRunning(display_link_));

    // See the comment on DisplayLinkMac::RegisterCallback() for not holding a
    // lock for CVDisplayLinkStop.
    CVReturn ret = CVDisplayLinkStop(display_link_);
    if (ret != kCVReturnSuccess) {
      LOG(ERROR) << "CVDisplayLinkStop failed. CVReturn: " << ret;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// VSyncCallbackMac

VSyncCallbackMac::VSyncCallbackMac(scoped_refptr<DisplayLinkMac> display_link,
                                   Callback callback,
                                   bool do_callback_on_ctor_thread)
    : display_link_(std::move(display_link)), weak_factory_(this) {
  if (do_callback_on_ctor_thread) {
    auto lambda = [](base::WeakPtr<VSyncCallbackMac> weak_this,
                     Callback callback, VSyncParamsMac params) {
      if (weak_this) {
        callback.Run(params);
      }
    };
    auto callback_for_current_thread =
        base::BindRepeating(lambda, weak_factory_.GetWeakPtr(), callback);
    callback_for_cvdisplaylink_thread_ =
        base::BindPostTaskToCurrentDefault(callback_for_current_thread);
  } else {
    callback_for_cvdisplaylink_thread_ = std::move(callback);
  }
}

VSyncCallbackMac::~VSyncCallbackMac() {
  display_link_->UnregisterCallback(this);
  display_link_ = nullptr;
}

}  // namespace ui
