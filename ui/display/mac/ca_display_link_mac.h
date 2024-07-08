// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace ui {
struct VSyncParamsMac;

// An implementation of ExternalBeginFrameSource which is driven by VSync
// signals coming from CADisplayLink.
class CADisplayLinkWrapper {
 public:
  using DisplayLinkCallback = base::RepeatingCallback<void(ui::VSyncParamsMac)>;

  static std::unique_ptr<CADisplayLinkWrapper> Create(
      CGDirectDisplayID display_id,
      DisplayLinkCallback callback);

  ~CADisplayLinkWrapper();

  void Start();
  void Stop();

  double GetRefreshRate();
  void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                               base::TimeDelta& max_interval,
                               base::TimeDelta& granularity);

  // Use the same minimum, maximum and preferred frame rate for the fixed frame
  // rate rerquest. If different minimum and maximum frame rates are set, the
  // actual callback rate will be dynamically adjusted to better align with
  // other animation sources.
  void SetPreferredIntervalRange(base::TimeDelta min_interval,
                                 base::TimeDelta max_interval,
                                 base::TimeDelta preferred_interval);

  // CVDisplayLink callback.
  void Step();

 private:
  struct ObjCState;

  explicit CADisplayLinkWrapper(std::unique_ptr<ObjCState> objc_state);
  CADisplayLinkWrapper(const CADisplayLinkWrapper&) = delete;
  CADisplayLinkWrapper& operator=(const CADisplayLinkWrapper&) = delete;

  // Return a nearest refresh interval that is supported by CADisplaylink.
  base::TimeDelta AdjustedToSupportedInterval(base::TimeDelta interval);

  std::unique_ptr<ObjCState> objc_state_;

  CGDirectDisplayID display_id_;

  bool paused_ = true;

  // The system can change the available range of frame rates because it factors
  // in system policies and a person’s preferences. For example, Low Power Mode,
  // critical thermal state, and accessibility settings can affect the system’s
  // frame rate. The system typically provides a consistent frame rate by
  // choosing one that’s a factor of the display’s maximum refresh rate.

  // The current frame interval range set in
  // CADisplayLink setPreferredFrameRateRange().
  base::TimeDelta preferred_interval_;
  base::TimeDelta max_interval_;
  base::TimeDelta min_interval_;

  DisplayLinkCallback callback_;
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
