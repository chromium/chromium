// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_CV_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CV_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>

#include <memory>
#include <set>
#include <vector>

#include "base/apple/scoped_typeref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/display/display_export.h"
#include "ui/display/mac/display_link_mac.h"

namespace ui {

class DisplayLinkMacSharedState;

// CVDisplayLinkMac indirectly owns a CVDisplayLink (via
// DisplayLinkMacSharedState), and may be used to create VSync callbacks.
class CVDisplayLinkMac : public DisplayLinkMac {
 public:
  // Create a CVDisplayLinkMac for the specified display.
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
  explicit CVDisplayLinkMac(DisplayLinkMacSharedState* shared_state);
  ~CVDisplayLinkMac() override;

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // A single DisplayLinkMacSharedState is shared between all CVDisplayLinkMac
  // instances that have same display ID. This is manually retained and
  // released.
  raw_ptr<DisplayLinkMacSharedState> shared_state_;
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CV_DISPLAY_LINK_MAC_H_
