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

class DISPLAY_EXPORT CADisplayLinkMac : public DisplayLinkMac {
 public:
  // Create a CADisplayLinkMac for the specified display.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // DisplayLinkMac implementation
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback) override;

  base::TimeDelta GetRefreshInterval() const override;
  void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                               base::TimeDelta& max_interval,
                               base::TimeDelta& granularity) const override;

  void SetPreferredInterval(base::TimeDelta interval) override {}

  base::TimeTicks GetCurrentTime() const override;

 private:
  explicit CADisplayLinkMac(CGDirectDisplayID display_id);
  ~CADisplayLinkMac() override;

  CADisplayLinkMac(const CADisplayLinkMac&) = delete;
  CADisplayLinkMac& operator=(const CADisplayLinkMac&) = delete;

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // CADisplayLink callback from ObjCState.display_link.
  void Step();

  const CGDirectDisplayID display_id_;
  std::unique_ptr<ObjCState> objc_state_;

  base::WeakPtr<VSyncCallbackMac> vsync_callback_;

  base::WeakPtrFactory<CADisplayLinkMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
