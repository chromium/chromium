// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>

#include <set>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/display/mac/display_link_mac.h"

namespace ui {
struct ObjCState;

class CADisplayLinkMac : public DisplayLinkMac {
 public:
  // Create a CADisplayLinkMac for the specified display.
  static scoped_refptr<DisplayLinkMac> GetForDisplayOnCurrentThread(
      CGDirectDisplayID display_id);

  // DisplayLinkMac implementation
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback) override;

  double GetRefreshRate() const override;
  void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                               base::TimeDelta& max_interval,
                               base::TimeDelta& granularity) const override;

  void SetPreferredInterval(base::TimeDelta interval) override;

  // Use the same minimum, maximum and preferred frame rate for the fixed frame
  // rate rerquest. If different minimum and maximum frame rates are set, the
  // actual callback rate will be dynamically adjusted to better align with
  // other animation sources.
  void SetPreferredIntervalRange(base::TimeDelta min_interval,
                                 base::TimeDelta max_interval,
                                 base::TimeDelta preferred_interval) override;

  base::TimeTicks GetCurrentTime() const override;

 private:
  explicit CADisplayLinkMac(CGDirectDisplayID display_id);
  ~CADisplayLinkMac() override;

  CADisplayLinkMac(const CADisplayLinkMac&) = delete;
  CADisplayLinkMac& operator=(const CADisplayLinkMac&) = delete;

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // Return a nearest refresh interval that is supported by CADisplaylink.
  base::TimeDelta AdjustedToSupportedInterval(base::TimeDelta interval);

  // CADisplayLink callback from ObjCState.display_link.
  void Step();

  const CGDirectDisplayID display_id_;
  std::unique_ptr<ObjCState> objc_state_;

  // The system can change the available range of frame rates because it factors
  // in system policies and a person’s preferences. For example, Low Power Mode,
  // critical thermal state, and accessibility settings can affect the system’s
  // frame rate. The system typically provides a consistent frame rate by
  // choosing one that’s a factor of the display’s maximum refresh rate.

  // The current frame interval range set in CADisplayLink
  // preferredFrameRateRange.
  base::TimeDelta preferred_interval_;
  base::TimeDelta max_interval_;
  base::TimeDelta min_interval_;

  base::WeakPtr<VSyncCallbackMac> vsync_callback_;

  // The number of consecutive DisplayLink VSyncs received after zero
  // |callbacks_|. DisplayLink will be stopped after |kMaxExtraVSyncs| is
  // reached.
  int consecutive_vsyncs_with_no_callbacks_ = 0;

  base::WeakPtrFactory<CADisplayLinkMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
