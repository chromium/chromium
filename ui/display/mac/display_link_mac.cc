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
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
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

}  // namespace

using DisplayLinkMap = std::map<CGDirectDisplayID, DisplayLinkMac*>;

namespace {

// The task runner to post tasks to from the display link thread. Note that this
// is initialized with the very first DisplayLinkMac instance, and is never
// changed (even, e.g, in tests that re-initialize the main thread task runner).
// https://885329
// TODO(ccameron): crbug.com/969157 - Save this ask_runner to DisplayLinkMac.
// configs += [ "//build/config/compiler:wexit_time_destructors" ] in
// ui/display/BUILD.gn has to be removed because GetMainThreadTaskRunner()
// causes a compiler error.
scoped_refptr<base::SingleThreadTaskRunner> GetMainThreadTaskRunner() {
  static scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  return task_runner;
}

// Each display link instance consumes a non-negligible number of cycles, so
// make all display links on the same screen share the same object.
//
// Note that this is a weak map, holding non-owning pointers to the
// DisplayLinkMac objects. DisplayLinkMac is a ref-counted class, and is
// jointly owned by the various callers that got a copy by calling
// GetForDisplay().
//
// ** This map may only be accessed from the main thread. **
DisplayLinkMap& GetAllDisplayLinks() {
  static base::NoDestructor<DisplayLinkMap> all_display_links;
  return *all_display_links;
}

}  // namespace

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id) {
  if (!display_id)
    return nullptr;

  // Return the existing display link for this display, if it exists.
  DisplayLinkMap& all_display_links = GetAllDisplayLinks();
  auto found = all_display_links.find(display_id);
  if (found != all_display_links.end())
    return found->second;

  CVReturn ret = kCVReturnSuccess;

  base::ScopedTypeRef<CVDisplayLinkRef> display_link;
  ret = CVDisplayLinkCreateWithCGDisplay(display_id,
                                         display_link.InitializeInto());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkCreateWithCGDisplay failed: " << ret;
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

  scoped_refptr<DisplayLinkMac> display_link_mac(
      new DisplayLinkMac(display_id, display_link));
  ret = CVDisplayLinkSetOutputCallback(display_link_mac->display_link_,
                                       &DisplayLinkCallback,
                                       reinterpret_cast<void*>(display_id));
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkSetOutputCallback failed: " << ret;
    return nullptr;
  }

  return display_link_mac;
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

DisplayLinkMac::DisplayLinkMac(
    CGDirectDisplayID display_id,
    base::ScopedTypeRef<CVDisplayLinkRef> display_link)
    : display_id_(display_id), display_link_(display_link) {
  DisplayLinkMap& all_display_links = GetAllDisplayLinks();
  DCHECK(all_display_links.find(display_id) == all_display_links.end());
  all_display_links.emplace(display_id_, this);
}

DisplayLinkMac::~DisplayLinkMac() {
  DCHECK(callbacks_.empty());

  DisplayLinkMap& all_display_links = GetAllDisplayLinks();
  auto found = all_display_links.find(display_id_);
  DCHECK(found != all_display_links.end());
  DCHECK(found->second == this);
  all_display_links.erase(found);
}

// static
void DisplayLinkMac::DisplayLinkCallbackOnMainThread(CGDirectDisplayID display,
                                                     VSyncParamsMac params) {
  DisplayLinkMap& all_display_links = GetAllDisplayLinks();
  auto found = all_display_links.find(display);
  if (found == all_display_links.end()) {
    // This might reasonably happen (and does; see https://crbug.com/564780). It
    // occasionally happens that the CVDisplayLink calls back on the video
    // thread, but by the time the callback makes it to the main thread for
    // processing, the DisplayLinkMac object has lost all its references and
    // has been deleted.
    return;
  }

  DisplayLinkMac* display_link_mac = found->second;
  display_link_mac->OnDisplayLinkCallback(params);
}

void DisplayLinkMac::OnDisplayLinkCallback(VSyncParamsMac params) {
  TRACE_EVENT0("ui", "DisplayLinkMac::OnDisplayLinkCallbackOnMainThread");

  auto callbacks_copy = callbacks_;
  for (auto* callback : callbacks_copy) {
    if (callbacks_.count(callback)) {
      callback->callback_.Run(params);
    }
  }
}

// static
CVReturn DisplayLinkMac::DisplayLinkCallback(CVDisplayLinkRef display_link,
                                             const CVTimeStamp* now,
                                             const CVTimeStamp* output_time,
                                             CVOptionFlags flags_in,
                                             CVOptionFlags* flags_out,
                                             void* context) {
  TRACE_EVENT0("ui", "DisplayLinkMac::DisplayLinkCallback");
  CGDirectDisplayID display_id =
      static_cast<CGDirectDisplayID>(reinterpret_cast<uintptr_t>(context));

  VSyncParamsMac params;
  params.callback_times_valid = ComputeVSyncParameters(
      *now, &params.callback_timebase, &params.callback_interval);
  params.display_times_valid = ComputeVSyncParameters(
      *output_time, &params.display_timebase, &params.display_interval);

  GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DisplayLinkMac::DisplayLinkCallbackOnMainThread,
                     display_id, params));
  return kCVReturnSuccess;
}

std::unique_ptr<VSyncCallbackMac> DisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback) {
  // Start the display link, if needed. If we fail to start the link, return
  // nullptr.
  if (callbacks_.empty()) {
    DCHECK(!CVDisplayLinkIsRunning(display_link_));

    if (!task_runner_) {
      task_runner_ = GetMainThreadTaskRunner();
    }

    CVReturn ret = CVDisplayLinkStart(display_link_);
    if (ret != kCVReturnSuccess) {
      LOG(ERROR) << "CVDisplayLinkStart failed: " << ret;
      return nullptr;
    }
  }

  std::unique_ptr<VSyncCallbackMac> new_observer(
      new VSyncCallbackMac(this, std::move(callback)));
  callbacks_.insert(new_observer.get());
  return new_observer;
}

void DisplayLinkMac::UnregisterCallback(VSyncCallbackMac* observer) {
  auto found = callbacks_.find(observer);
  CHECK(found != callbacks_.end());
  callbacks_.erase(found);

  // Stop the CVDisplayLink if all observers are removed.
  if (callbacks_.empty()) {
    DCHECK(CVDisplayLinkIsRunning(display_link_));
    CVReturn ret = CVDisplayLinkStop(display_link_);
    if (ret != kCVReturnSuccess) {
      LOG(ERROR) << "CVDisplayLinkStop failed: " << ret;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// VSyncCallbackMac

VSyncCallbackMac::VSyncCallbackMac(scoped_refptr<DisplayLinkMac> display_link,
                                   Callback callback)
    : display_link_(std::move(display_link)), callback_(std::move(callback)) {}

VSyncCallbackMac::~VSyncCallbackMac() {
  display_link_->UnregisterCallback(this);
  display_link_ = nullptr;
}

}  // namespace ui
