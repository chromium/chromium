// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_CV_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CV_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>
#import <QuartzCore/CVDisplayLink.h>

#include <memory>
#include <set>
#include <vector>

#include "base/apple/scoped_typeref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "ui/display/display_export.h"
#include "ui/display/mac/display_link_mac.h"

namespace ui {

class DisplayLinkMacSharedState;

// CVDisplayLinkMac indirectly owns a CVDisplayLink (via
// DisplayLinkMacSharedState), and may be used to create VSync callbacks.
class CVDisplayLinkMac : public DisplayLinkMac {
 public:
  // Create a CVDisplayLinkMac for the specified display. The returned
  // object will be shared across all callers on a given calling thread.
  static scoped_refptr<CVDisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // DisplayLinkMac implementation
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback) override;

  double GetRefreshRate() const override;
  void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                               base::TimeDelta& max_interval,
                               base::TimeDelta& granularity) const override;

  void SetPreferredInterval(base::TimeDelta interval) override {}
  void SetPreferredIntervalRange(base::TimeDelta min_interval,
                                 base::TimeDelta max_interval,
                                 base::TimeDelta preferred_interval) override {}

  // Retrieves the current (“now”) time of a given display link. Returns
  // base::TimeTicks() if the current time is not available.
  base::TimeTicks GetCurrentTime() const override;

 private:
  CVDisplayLinkMac(CGDirectDisplayID display_id,
                   base::PlatformThreadId thread_id,
                   base::apple::ScopedTypeRef<CVDisplayLinkRef> display_link);
  ~CVDisplayLinkMac() override;

  // The EnsureDisplayLinkRunning call will return false if DisplayLinkStart
  // fails.
  bool EnsureDisplayLinkRunning();
  void StopDisplayLinkIfNeeded();

  // The static callback function called by the CVDisplayLink, on the
  // CVDisplayLink thread.
  static CVReturn CVDisplayLinkCallback(CVDisplayLinkRef display_link,
                                        const CVTimeStamp* now,
                                        const CVTimeStamp* output_time,
                                        CVOptionFlags flags_in,
                                        CVOptionFlags* flags_out,
                                        void* context);

  // The static callback function called on `thread_id`.
  static void CVDisplayLinkCallbackOnCallbackThread(
      CGDirectDisplayID display_id,
      base::PlatformThreadId thread_id,
      const VSyncParamsMac& params);

  // The non-static function called called on vsync tick.
  void RunCallbacks(const VSyncParamsMac& params);

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // The display that this display link is attached to.
  const CGDirectDisplayID display_id_;

  // The ID of the thread that `this` may be accessed on.
  const base::PlatformThreadId thread_id_;

  // CVDisplayLink for querying VSync timing info.
  base::apple::ScopedTypeRef<CVDisplayLinkRef> display_link_;
  bool display_link_is_running_ = false;

  // Each VSyncCallbackMac holds a reference to `this`.
  std::set<VSyncCallbackMac*> callbacks_;

  // The number of consecutive DisplayLink VSyncs received after zero
  // |callbacks_|. DisplayLink will be stopped after |kMaxExtraVSyncs| is
  // reached. It's guarded by |globals.lock|.
  int consecutive_vsyncs_with_no_callbacks_ = 0;

  // The task runner for the thread on which this is called and on which all
  // callbacks will be made.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CV_DISPLAY_LINK_MAC_H_
