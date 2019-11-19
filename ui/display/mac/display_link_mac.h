// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_

#include <QuartzCore/CVDisplayLink.h>

#include <map>

#include "base/mac/scoped_typeref.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/display/display_export.h"

namespace ui {

class DISPLAY_EXPORT DisplayLinkMac
    : public base::RefCountedThreadSafe<DisplayLinkMac> {
 public:
  // This must only be called from the main thread.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // Get vsync scheduling parameters. Returns false if the populated parameters
  // are invalid.
  bool GetVSyncParameters(base::TimeTicks* timebase, base::TimeDelta* interval);

  // Get the panel/monitor refresh rate
  double GetRefreshRate();

 private:
  friend class base::RefCountedThreadSafe<DisplayLinkMac>;

  DisplayLinkMac(CGDirectDisplayID display_id,
                 base::ScopedTypeRef<CVDisplayLinkRef> display_link);
  virtual ~DisplayLinkMac();

  void StartOrContinueDisplayLink();
  void StopDisplayLink();

  // Looks up the display and calls UpdateVSyncParameters() on the corresponding
  // DisplayLinkMac.
  static void DoUpdateVSyncParameters(CGDirectDisplayID display,
                                      const CVTimeStamp& time);

  // Processes the display link callback.
  void UpdateVSyncParameters(const CVTimeStamp& time);

  // Called by the system on the display link thread, and posts a call to
  // DoUpdateVSyncParameters() to the UI thread.
  static CVReturn DisplayLinkCallback(CVDisplayLinkRef display_link,
                                      const CVTimeStamp* now,
                                      const CVTimeStamp* output_time,
                                      CVOptionFlags flags_in,
                                      CVOptionFlags* flags_out,
                                      void* context);

  // This is called whenever the display is reconfigured, and marks that the
  // vsync parameters must be recalculated.
  static void DisplayReconfigurationCallBack(CGDirectDisplayID display,
                                             CGDisplayChangeSummaryFlags flags,
                                             void* user_info);

  // The display that this display link is attached to.
  CGDirectDisplayID display_id_;

  // CVDisplayLink for querying VSync timing info.
  base::ScopedTypeRef<CVDisplayLinkRef> display_link_;

  // The task runner to post tasks to from the display link thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // VSync parameters computed during UpdateVSyncParameters().
  bool timebase_and_interval_valid_ = false;
  base::TimeTicks timebase_;
  base::TimeDelta interval_;

  // The time after which we should re-start the display link to get fresh
  // parameters.
  base::TimeTicks recalculate_time_;
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
