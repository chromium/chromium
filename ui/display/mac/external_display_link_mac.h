// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_EXTERNAL_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_EXTERNAL_DISPLAY_LINK_MAC_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "ui/display/mac/display_link_mac.h"
#include "ui/display/mac/vsync_provider_mac.h"

namespace ui {

// Unlike CADisplayLinkMac and CVDisplayLinkMac, ExternalDisplayLinkMac itself
// does not register a CADisplaylink directly from CoreAnimation. Instead,
// ExternalDisplayLinkMac creates an DisplayLinkMac object and gets its VSync
// from VSyncProviderMac.
class ExternalDisplayLinkMac : public DisplayLinkMac {
 public:
  // Return a new ExternalDisplayLinkMac for each call.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(int64_t display_id);

  static bool IsDisplayLinkSupported(int64_t display_id);

  // DisplayLinkMac implementation
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback) override;

  base::TimeDelta GetRefreshInterval() const override;
  void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                               base::TimeDelta& max_interval,
                               base::TimeDelta& granularity) const override;

  void SetPreferredInterval(base::TimeDelta interval) override {}

  // Retrieves the current (“now”) time of a given display link.
  base::TimeTicks GetCurrentTime() const override;

 private:
  explicit ExternalDisplayLinkMac(int64_t display_id);
  ~ExternalDisplayLinkMac() override;

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // The display this display link is attached to.
  const int64_t display_id_;

  raw_ptr<VSyncProviderMac> vsync_provider_;

  const bool post_callback_to_ctor_thread_ = false;

  VSyncCallbackMac::Callback callback_for_providers_thread_;

  base::WeakPtrFactory<ExternalDisplayLinkMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_EXTERNAL_DISPLAY_LINK_MAC_H_
