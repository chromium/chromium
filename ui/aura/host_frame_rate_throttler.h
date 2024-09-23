// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_HOST_FRAME_RATE_THROTTLER_H_
#define UI_AURA_HOST_FRAME_RATE_THROTTLER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "ui/aura/aura_export.h"

namespace aura {

class WindowTreeHost;

// Used to throttle the frame rate of hosts that are occluded.
class AURA_EXPORT HostFrameRateThrottler {
 public:
  static HostFrameRateThrottler& GetInstance();

  void AddHost(WindowTreeHost* host);
  void RemoveHost(WindowTreeHost* host);

  const base::flat_set<raw_ptr<WindowTreeHost, CtnExperimental>>& hosts()
      const {
    return hosts_;
  }

 private:
  friend class base::NoDestructor<HostFrameRateThrottler>;

  HostFrameRateThrottler();
  ~HostFrameRateThrottler();

  void UpdateHostFrameSinkManager();

  // Set of hosts that are currently throttled.
  base::flat_set<raw_ptr<WindowTreeHost, CtnExperimental>> hosts_;
};

}  // namespace aura

#endif  // UI_AURA_HOST_FRAME_RATE_THROTTLER_H_
