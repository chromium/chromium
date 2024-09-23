// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/cv_display_link_mac.h"

#import <QuartzCore/CVDisplayLink.h>
#include <stdint.h>

#include <set>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"

namespace base::apple {

template <>
struct ScopedTypeRefTraits<CVDisplayLinkRef> {
  static CVDisplayLinkRef InvalidValue() { return nullptr; }
  static CVDisplayLinkRef Retain(CVDisplayLinkRef object) {
    return CVDisplayLinkRetain(object);
  }
  static void Release(CVDisplayLinkRef object) { CVDisplayLinkRelease(object); }
};

}  // namespace base::apple

namespace ui {

namespace {

struct DisplayLinkGlobals {
  // A map from (display ID, thread ID) pairs to raw CVDisplayLink pointers.
  std::map<std::pair<CGDirectDisplayID, base::PlatformThreadId>,
           raw_ptr<CVDisplayLinkMac>>
      GUARDED_BY(lock) map;

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

}  // namespace

// Called by the system on the CVDisplayLink thread, and posts a call to the
// thread indicated in CVDisplayLinkMac::RegisterCallback().
CVReturn CVDisplayLinkMac::CVDisplayLinkCallback(CVDisplayLinkRef display_link,
                                                 const CVTimeStamp* now,
                                                 const CVTimeStamp* output_time,
                                                 CVOptionFlags flags_in,
                                                 CVOptionFlags* flags_out,
                                                 void* context) {
  // This function is called on the system display link thread.
  TRACE_EVENT0("ui", "DisplayLinkCallback");

  // Convert the time parameters to our VSync parameters.
  VSyncParamsMac params;
  params.callback_times_valid = ComputeVSyncParameters(
      *now, &params.callback_timebase, &params.callback_interval);
  params.display_times_valid = ComputeVSyncParameters(
      *output_time, &params.display_timebase, &params.display_interval);

  // Post a task to the thread on which callbacks are to be run. It is safe to
  // access `display_link_mac` as a raw pointer because it is guaranteed that
  // this function will not be called after `display_link_mac->display_link_`
  // has been stopped (which happens in the CVDisplayLinkMac destructor).
  CVDisplayLinkMac* display_link_mac =
      reinterpret_cast<CVDisplayLinkMac*>(context);
  display_link_mac->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CVDisplayLinkMac::CVDisplayLinkCallbackOnCallbackThread,
                     display_link_mac->display_id_,
                     display_link_mac->thread_id_, params));
  return kCVReturnSuccess;
}

// static
void CVDisplayLinkMac::CVDisplayLinkCallbackOnCallbackThread(
    CGDirectDisplayID display_id,
    base::PlatformThreadId thread_id,
    const VSyncParamsMac& params) {
  scoped_refptr<CVDisplayLinkMac> display_link;

  // Look up the CVDisplayLinkMac that this call was made for. It may no longer
  // exist.
  {
    auto key = std::make_pair(display_id, base::PlatformThread::CurrentId());
    auto& globals = DisplayLinkGlobals::Get();
    base::AutoLock lock(globals.lock);
    auto found = globals.map.find(key);
    if (found == globals.map.end()) {
      return;
    }
    display_link = found->second;
  }

  display_link->RunCallbacks(params);
}

// static
scoped_refptr<CVDisplayLinkMac> CVDisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id) {
  const auto thread_id = base::PlatformThread::CurrentId();

  // If there already exists an object for this display on this thread, return
  // it.
  {
    auto& globals = DisplayLinkGlobals::Get();
    base::AutoLock lock(globals.lock);
    auto found = globals.map.find(std::make_pair(display_id, thread_id));
    if (found != globals.map.end()) {
      return found->second.get();
    }
  }

  // Create the CVDisplayLink object
  CVReturn ret = kCVReturnSuccess;
  base::apple::ScopedTypeRef<CVDisplayLinkRef> display_link;
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
      (CVDisplayLinkGetCurrentCGDisplay(display_link.get()) == 0)) {
    LOG(ERROR)
        << "CVDisplayLinkCreateWithCGDisplay failed (no current display)";
    return nullptr;
  }

  scoped_refptr<CVDisplayLinkMac> result =
      new CVDisplayLinkMac(display_id, thread_id, display_link);

  ret = CVDisplayLinkSetOutputCallback(display_link.get(),
                                       &CVDisplayLinkMac::CVDisplayLinkCallback,
                                       result.get());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkSetOutputCallback failed. CVReturn: " << ret;
    return nullptr;
  }

  return result;
}

// Functions to call CVDisplayLinkStart and CVDisplayLinkStop. This is
// reference counted, and takes `display_link_running_lock_`.
bool CVDisplayLinkMac::EnsureDisplayLinkRunning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!display_link_is_running_) {
    DCHECK(!CVDisplayLinkIsRunning(display_link_.get()));
    CVReturn ret = CVDisplayLinkStart(display_link_.get());
    if (ret != kCVReturnSuccess) {
      LOG(ERROR) << "CVDisplayLinkStart failed. CVReturn: " << ret;
      return false;
    }

    display_link_is_running_ = true;
  }

  return true;
}

// Called on the system CVDisplayLink thread.
void CVDisplayLinkMac::StopDisplayLinkIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!callbacks_.empty()) {
    consecutive_vsyncs_with_no_callbacks_ = 0;
    return;
  }
  consecutive_vsyncs_with_no_callbacks_ += 1;
  if (consecutive_vsyncs_with_no_callbacks_ <
      VSyncCallbackMac::kMaxExtraVSyncs) {
    return;
  }

  if (!display_link_is_running_) {
    DCHECK(!CVDisplayLinkIsRunning(display_link_.get()));
    return;
  }

  CVReturn ret = CVDisplayLinkStop(display_link_.get());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkStop failed. CVReturn: " << ret;
  }

  display_link_is_running_ = false;
  consecutive_vsyncs_with_no_callbacks_ = 0;
}

double CVDisplayLinkMac::GetRefreshRate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  double refresh_rate = 0;

  CVTime cv_time =
      CVDisplayLinkGetNominalOutputVideoRefreshPeriod(display_link_.get());
  if (!(cv_time.flags & kCVTimeIsIndefinite)) {
    refresh_rate = (static_cast<double>(cv_time.timeScale) /
                    static_cast<double>(cv_time.timeValue));
  }

  return refresh_rate;
}

void CVDisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  double refresh_rate = GetRefreshRate();
  if (refresh_rate) {
    min_interval = base::Seconds(1) / refresh_rate;
    max_interval = min_interval;
    granularity = min_interval;
  } else {
    min_interval = base::TimeDelta();
    max_interval = base::TimeDelta();
  }
}

base::TimeTicks CVDisplayLinkMac::GetCurrentTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CVTimeStamp out_time;
  CVReturn ret = CVDisplayLinkGetCurrentTime(display_link_.get(), &out_time);
  if (ret == kCVReturnSuccess) {
    return base::TimeTicks::FromMachAbsoluteTime(out_time.hostTime);
  } else {
    return base::TimeTicks();
  }
}

void CVDisplayLinkMac::RunCallbacks(const VSyncParamsMac& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* callback : callbacks_) {
    callback->callback_for_displaylink_thread_.Run(params);
  }

  StopDisplayLinkIfNeeded();
}

CVDisplayLinkMac::CVDisplayLinkMac(
    CGDirectDisplayID display_id,
    base::PlatformThreadId thread_id,
    base::apple::ScopedTypeRef<CVDisplayLinkRef> display_link)
    : display_id_(display_id),
      thread_id_(thread_id),
      display_link_(display_link),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // Insert `this` into the global map. There must be no conflicting entry
  // present.
  {
    auto key = std::make_pair(display_id, base::PlatformThread::CurrentId());
    auto& globals = DisplayLinkGlobals::Get();
    base::AutoLock lock(globals.lock);
    auto result = globals.map.insert(std::make_pair(key, this));
    bool inserted = result.second;
    DCHECK(inserted);
  }
}

CVDisplayLinkMac::~CVDisplayLinkMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // All callbacks hold a reference to `this`, so there can be none.
  DCHECK(callbacks_.empty());

  // Stop the display link (if needed). After this call, it is safe to assume
  // that CVDisplayLinkMac::CVDisplayLinkCallback will not be called with
  // `this`.
  if (display_link_is_running_) {
    CVReturn ret = CVDisplayLinkStop(display_link_.get());
    if (ret != kCVReturnSuccess) {
      LOG(ERROR) << "CVDisplayLinkStop failed. CVReturn: " << ret;
    }
  }

  // Remove `this` from the global map. It must be present.
  {
    auto key = std::make_pair(display_id_, thread_id_);
    auto& globals = DisplayLinkGlobals::Get();
    base::AutoLock lock(globals.lock);
    auto found = globals.map.find(key);
    DCHECK(found != globals.map.end());
    DCHECK(found->second == this);
    globals.map.erase(found);
  }
}

std::unique_ptr<VSyncCallbackMac> CVDisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make add the new callback. Register first before calling
  // EnsureDisplayLinkRunning() to ensure the callback function is available.

  std::unique_ptr<VSyncCallbackMac> new_callback =
      base::WrapUnique(new VSyncCallbackMac(
          base::BindOnce(&CVDisplayLinkMac::UnregisterCallback, this),
          std::move(callback), /*post_callback_to_ctor_thread=*/true));

  // Ensure that the DisplayLink is running. If something goes wrong, return
  // nullptr.
  if (!EnsureDisplayLinkRunning()) {
    new_callback.reset();
  } else {
    callbacks_.insert(new_callback.get());
  }

  return new_callback;
}

void CVDisplayLinkMac::UnregisterCallback(VSyncCallbackMac* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.erase(callback);
}

}  // namespace ui
