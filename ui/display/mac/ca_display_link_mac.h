// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/display/mac/display_link_mac.h"

namespace ui {
class Wrapper;

class CADisplayLinkMac : public DisplayLinkMac {
 public:
  // Create a CVDisplayLinkMac for the specified display.
  static scoped_refptr<CADisplayLinkMac> GetForDisplayOnCurrentThread(
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
  explicit CADisplayLinkMac(Wrapper* wrapper);
  ~CADisplayLinkMac() override;

  CADisplayLinkMac(const CADisplayLinkMac&) = delete;
  CADisplayLinkMac& operator=(const CADisplayLinkMac&) = delete;

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // Return a nearest refresh interval that is supported by CADisplaylink.
  base::TimeDelta AdjustedToSupportedInterval(base::TimeDelta interval);

  // A single Wrapper is shared between all CADisplayLinkMac instances that have
  // same display ID on the same thread. This is manually retained and released.
  raw_ptr<Wrapper> wrapper_;

  base::WeakPtrFactory<CADisplayLinkMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
