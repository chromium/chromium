// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_CA_METAL_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CA_METAL_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/display/mac/display_link_mac.h"

namespace ui {
struct MetalObjCState;

class API_AVAILABLE(macos(14.0)) DISPLAY_EXPORT CAMetalDisplayLinkMac
    : public DisplayLinkMac {
 public:
  // Create a CAMetalDisplayLinkMac for the specified display.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  static scoped_refptr<DisplayLinkMac> MetalGetForDisplay(
      CGDirectDisplayID display_id,
      CAMetalLayer* metal_layer);

  // DisplayLinkMac implementation
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback) override;

  std::unique_ptr<PresentationCallbackMac> RegisterPresentationCallback(
      PresentationCallbackMac::Callback callback) override;

  base::TimeDelta GetRefreshInterval() const override;
  void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                               base::TimeDelta& max_interval,
                               base::TimeDelta& granularity) const override;

  void SetPreferredInterval(base::TimeDelta interval) override {}

  base::TimeTicks GetCurrentTime() const override;

 private:
  explicit CAMetalDisplayLinkMac(CGDirectDisplayID display_id);
  ~CAMetalDisplayLinkMac() override;

  CAMetalDisplayLinkMac(const CAMetalDisplayLinkMac&) = delete;
  CAMetalDisplayLinkMac& operator=(const CAMetalDisplayLinkMac&) = delete;

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);
  void UnregisterPresentationCallback(PresentationCallbackMac* callback);

  // CADisplayLink callback from MetalObjCState.display_link.
  void MetalDisplayLinkCallback(CAMetalDisplayLink* display_link,
                                CAMetalDisplayLinkUpdate* update);

  void MetalPresentationCallback(id<MTLDrawable> drawable);

  const CGDirectDisplayID display_id_;
  std::unique_ptr<MetalObjCState> objc_state_;

  base::WeakPtr<VSyncCallbackMac> vsync_callback_;

  base::WeakPtr<PresentationCallbackMac> presented_callback_;

  // This is used for calculating the frame interval (target_time -
  // last_target_time), valid only when the callbacks are continues. If callback
  // just started, we cannot use the first callback times -|last_target_time_|
  // recorded before the pause to calculate the interval.
  base::TimeTicks last_target_time_;
  bool last_target_time_is_valid_ = false;

  base::WeakPtrFactory<CAMetalDisplayLinkMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CA_METAL_DISPLAY_LINK_MAC_H_
