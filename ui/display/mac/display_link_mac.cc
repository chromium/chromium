// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/display_link_mac.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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

// The task runner to post tasks to from the display link thread. Note that this
// is initialized with the very first DisplayLinkMac instance, and is never
// changed (even, e.g, in tests that re-initialize the main thread task runner).
// https://885329
// TODO(ccameron): crbug.com/969157 - Save this ask_runner to DisaplayLinkMac.
// configs += [ "//build/config/compiler:wexit_time_destructors" ] in
// ui/display/BUILD.gn has to be removed because GetMainThreadTaskRunner()
// causes a compiler error.
scoped_refptr<base::SingleThreadTaskRunner> GetMainThreadTaskRunner() {
  static scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
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
    LOG(ERROR) << "CVDisplayLinkCreateWithActiveCGDisplays failed: " << ret;
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

bool DisplayLinkMac::GetVSyncParameters(base::TimeTicks* timebase,
                                        base::TimeDelta* interval) {
  if (!timebase_and_interval_valid_) {
    StartOrContinueDisplayLink();
    return false;
  }

  // The vsync parameters skew over time (astonishingly quickly -- 0.1 msec per
  // second). If too much time has elapsed since the last time the vsync
  // parameters were calculated, re-calculate them (but still return the old
  // parameters -- the update will be asynchronous).
  if (base::TimeTicks::Now() >= recalculate_time_)
    StartOrContinueDisplayLink();

  *timebase = timebase_;
  *interval = interval_;
  return true;
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
  if (all_display_links.empty()) {
    CGError register_error = CGDisplayRegisterReconfigurationCallback(
        DisplayReconfigurationCallBack, nullptr);
    DPLOG_IF(ERROR, register_error != kCGErrorSuccess)
        << "CGDisplayRegisterReconfigurationCallback: " << register_error;
  }
  all_display_links.emplace(display_id_, this);
}

DisplayLinkMac::~DisplayLinkMac() {
  StopDisplayLink();

  DisplayLinkMap& all_display_links = GetAllDisplayLinks();
  auto found = all_display_links.find(display_id_);
  DCHECK(found != all_display_links.end());
  DCHECK(found->second == this);
  all_display_links.erase(found);
  if (all_display_links.empty()) {
    CGError remove_error = CGDisplayRemoveReconfigurationCallback(
        DisplayReconfigurationCallBack, nullptr);
    DPLOG_IF(ERROR, remove_error != kCGErrorSuccess)
        << "CGDisplayRemoveReconfigurationCallback: " << remove_error;
  }
}

// static
void DisplayLinkMac::DoUpdateVSyncParameters(CGDirectDisplayID display,
                                             const CVTimeStamp& time) {
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
  display_link_mac->UpdateVSyncParameters(time);
}

void DisplayLinkMac::UpdateVSyncParameters(const CVTimeStamp& cv_time) {
  TRACE_EVENT0("ui", "DisplayLinkMac::UpdateVSyncParameters");

  // Verify that videoRefreshPeriod fits in 32 bits.
  DCHECK((cv_time.videoRefreshPeriod & ~0xFFFF'FFFFull) == 0ull);

  // Verify that the numerator and denominator make some sense.
  uint32_t numerator = static_cast<uint32_t>(cv_time.videoRefreshPeriod);
  uint32_t denominator = cv_time.videoTimeScale;
  if (numerator == 0 || denominator == 0) {
    LOG(WARNING) << "Unexpected numerator or denominator, bailing.";
    return;
  }

  base::CheckedNumeric<int64_t> interval_us(base::Time::kMicrosecondsPerSecond);
  interval_us *= numerator;
  interval_us /= denominator;
  if (!interval_us.IsValid()) {
    LOG(DFATAL) << "Bailing due to overflow: "
                << base::Time::kMicrosecondsPerSecond << " * " << numerator
                << " / " << denominator;
    return;
  }

  timebase_ = base::TimeTicks::FromMachAbsoluteTime(cv_time.hostTime);
  interval_ = base::TimeDelta::FromMicroseconds(interval_us.ValueOrDie());
  timebase_and_interval_valid_ = true;

  // Don't restart the display link for 10 seconds.
  recalculate_time_ = base::TimeTicks::Now() + base::TimeDelta::FromSeconds(10);
  StopDisplayLink();
}

void DisplayLinkMac::StartOrContinueDisplayLink() {
  if (CVDisplayLinkIsRunning(display_link_))
    return;

  // Ensure the main thread is captured.
  if (!task_runner_)
    task_runner_ = GetMainThreadTaskRunner();

  CVReturn ret = CVDisplayLinkStart(display_link_);
  if (ret != kCVReturnSuccess)
    LOG(ERROR) << "CVDisplayLinkStart failed: " << ret;
}

void DisplayLinkMac::StopDisplayLink() {
  if (!CVDisplayLinkIsRunning(display_link_))
    return;

  CVReturn ret = CVDisplayLinkStop(display_link_);
  if (ret != kCVReturnSuccess)
    LOG(ERROR) << "CVDisplayLinkStop failed: " << ret;
}

// static
CVReturn DisplayLinkMac::DisplayLinkCallback(CVDisplayLinkRef display_link,
                                             const CVTimeStamp* now,
                                             const CVTimeStamp* output_time,
                                             CVOptionFlags flags_in,
                                             CVOptionFlags* flags_out,
                                             void* context) {
  TRACE_EVENT0("ui", "DisplayLinkMac::DisplayLinkCallback");
  CGDirectDisplayID display =
      static_cast<CGDirectDisplayID>(reinterpret_cast<uintptr_t>(context));
  GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&DisplayLinkMac::DoUpdateVSyncParameters,
                                display, *output_time));
  return kCVReturnSuccess;
}

// static
void DisplayLinkMac::DisplayReconfigurationCallBack(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void* user_info) {
  DisplayLinkMap& all_display_links = GetAllDisplayLinks();
  auto found = all_display_links.find(display);
  if (found == all_display_links.end())
    return;

  DisplayLinkMac* display_link_mac = found->second;
  display_link_mac->timebase_and_interval_valid_ = false;
}

}  // namespace ui
