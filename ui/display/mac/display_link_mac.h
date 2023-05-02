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
// See notes in DisplayLinkMac::RegisterCallback
class DISPLAY_EXPORT VSyncCallbackMac {
 public:
  using Callback = base::RepeatingCallback<void(VSyncParamsMac)>;
  ~VSyncCallbackMac();

 private:
  friend class DisplayLinkMac;
  VSyncCallbackMac(scoped_refptr<DisplayLinkMac> display_link,
                   Callback callback,
                   bool do_callback_on_ctor_thread);

  // The DisplayLinkMac that `this` is observing is kept alive while `this` is
  // alive.
  scoped_refptr<DisplayLinkMac> display_link_;

  // The callback that will be run on the CVDisplayLink thread. If `this` was
  // created with `do_callback_on_ctor_thread`, then this callback will post a
  // task to the creating thread,
  Callback callback_for_cvdisplaylink_thread_;

  base::WeakPtrFactory<VSyncCallbackMac> weak_factory_;
};

class DISPLAY_EXPORT DisplayLinkMac
    : public base::RefCountedThreadSafe<DisplayLinkMac> {
 public:
  // Get the DisplayLinkMac for the specified display.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // Register an observer callback.
  // * The specified callback will be called at every VSync tick, until the
  //   returned VSyncCallbackMac object is destroyed.
  // * The resulting VSyncCallbackMac object must be destroyed on the same
  //   thread on which it was created.
  // * If `do_callback_on_register_thread` is true, then the callback is
  //   guaranteed to be made on the calling thread and is guaranteed to be made
  //   only if the resulting VSyncCallbackMac has not been destroyed.
  // * If `do_callback_on_register_thread` is false then the callback may come
  //   from any thread, and may happen after the resulting VSyncCallbackMac is
  //   destroyed.
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback,
      bool do_callback_on_register_thread = true);

  // Get the panel/monitor refresh rate
  double GetRefreshRate();

 private:
  friend class base::RefCountedThreadSafe<DisplayLinkMac>;
  friend class VSyncCallbackMac;

  DisplayLinkMac(CGDirectDisplayID display_id,
                 base::ScopedTypeRef<CVDisplayLinkRef> display_link);
  virtual ~DisplayLinkMac();

  void UnregisterCallback(VSyncCallbackMac* callback);

  // Called by the system on the display link thread, and posts a call to
  // the thread indicated in DisplayLinkMac::RegisterCallback().
  static CVReturn DisplayLinkCallback(CVDisplayLinkRef display_link,
                                      const CVTimeStamp* now,
                                      const CVTimeStamp* output_time,
                                      CVOptionFlags flags_in,
                                      CVOptionFlags* flags_out,
                                      void* context);

  // The display that this display link is attached to.
  CGDirectDisplayID display_id_;

  // CVDisplayLink for querying VSync timing info.
  base::ScopedTypeRef<CVDisplayLinkRef> display_link_;

  // Each VSyncCallbackMac holds a reference to `this`. This member may
  // be accessed on any thread while |globals.lock| is held. But it can only be
  // modified on the main thread. |globals.lock| also guards DisplayLinkMac map.
  std::set<VSyncCallbackMac*> callbacks_;

  SEQUENCE_CHECKER(display_link_mac_sequence_checker_);
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
