// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_

#include <QuartzCore/CVDisplayLink.h>

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/mac/scoped_typeref.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/display/display_export.h"

namespace ui {

class DisplayLinkMac;

// VSync parameters parsed from CVDisplayLinkOutputCallback's parameters.
struct DISPLAY_EXPORT VSyncParamsMac {
  // The time of the callback.
  bool callback_times_valid = false;
  base::TimeTicks callback_timebase;
  base::TimeDelta callback_interval;

  // The indicated display time.
  bool display_times_valid = false;
  base::TimeTicks display_timebase;
  base::TimeDelta display_interval;
};

// Object used to control the lifetime of callbacks from DisplayLinkMac.
class DISPLAY_EXPORT VSyncCallbackMac {
 public:
  using Callback = base::RepeatingCallback<void(VSyncParamsMac)>;
  ~VSyncCallbackMac();

 private:
  friend class DisplayLinkMac;
  VSyncCallbackMac(scoped_refptr<DisplayLinkMac> display_link,
                   Callback callback);

  // The DisplayLinkMac that `this` is observing is kept alive while `this` is
  // alive.
  scoped_refptr<DisplayLinkMac> display_link_;
  Callback callback_;
};

class DISPLAY_EXPORT DisplayLinkMac
    : public base::RefCountedThreadSafe<DisplayLinkMac> {
 public:
  // Get the DisplayLinkMac for the specified display. All calls to this
  // function (and all other functions on this class) must be on the same
  // thread.
  // TODO(https://crbug.com/1419870): Remove this restriction.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // Register an observer callback. The specified callback will be called at
  // every VSync tick until the returned object is destroyed.
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback);

  // Get the panel/monitor refresh rate
  double GetRefreshRate();

 private:
  friend class base::RefCountedThreadSafe<DisplayLinkMac>;
  friend class VSyncCallbackMac;

  DisplayLinkMac(CGDirectDisplayID display_id,
                 base::ScopedTypeRef<CVDisplayLinkRef> display_link);
  virtual ~DisplayLinkMac();

  void OnDisplayLinkCallback(VSyncParamsMac params);
  void UnregisterCallback(VSyncCallbackMac* callback);

  // Called by the system on the display link thread, and posts a call to
  // DoUpdateVSyncParameters() to the UI thread.
  static CVReturn DisplayLinkCallback(CVDisplayLinkRef display_link,
                                      const CVTimeStamp* now,
                                      const CVTimeStamp* output_time,
                                      CVOptionFlags flags_in,
                                      CVOptionFlags* flags_out,
                                      void* context);

  // Looks up the display and calls UpdateVSyncParameters() on the corresponding
  // DisplayLinkMac.
  static void DisplayLinkCallbackOnMainThread(CGDirectDisplayID display,
                                              VSyncParamsMac params);

  // The display that this display link is attached to.
  CGDirectDisplayID display_id_;

  // CVDisplayLink for querying VSync timing info.
  base::ScopedTypeRef<CVDisplayLinkRef> display_link_;

  // The task runner to post tasks to from the display link thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Each VSyncCallbackMac holds a reference to `this`.
  std::set<VSyncCallbackMac*> callbacks_;
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
