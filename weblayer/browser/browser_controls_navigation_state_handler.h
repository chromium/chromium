// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_H_
#define WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_H_

#include "base/optional.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_controls_state.h"

namespace weblayer {

class BrowserControlsNavigationStateHandlerDelegate;

// BrowserControlsNavigationStateHandler is responsible for the tracking the
// value of content::BrowserControlsState as related to navigation state and
// notifying the delegate when the state changes.
//
// This class is roughly a combination of TopControlsSliderControllerChromeOS
// and TabStateBrowserControlsVisibilityDelegate.
class BrowserControlsNavigationStateHandler
    : public content::WebContentsObserver {
 public:
  BrowserControlsNavigationStateHandler(
      content::WebContents* web_contents,
      BrowserControlsNavigationStateHandlerDelegate* delegate);
  BrowserControlsNavigationStateHandler(
      const BrowserControlsNavigationStateHandler&) = delete;
  BrowserControlsNavigationStateHandler& operator=(
      const BrowserControlsNavigationStateHandler&) = delete;
  ~BrowserControlsNavigationStateHandler() override;

  // Returns true if the renderer is responsible for controlling the offsets.
  // This is normally true, but if the renderer is unresponsive (hung and/or
  // crashed), then the renderer won't be able to drive the offsets.
  bool IsRendererControllingOffsets();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidChangeVisibleSecurityState() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void OnRendererUnresponsive(
      content::RenderProcessHost* render_process_host) override;
  void OnRendererResponsive(
      content::RenderProcessHost* render_process_host) override;

 private:
  // Sets the value of |force_show_during_load_|. Calls to UpdateState() if
  // the value changed.
  void SetForceShowDuringLoad(bool value);

  // Schedules a timer to set |force_show_during_load_| to false.
  void ScheduleStopDelayedForceShow();

  // Checks the current state, and if it has changed notifies the delegate.
  void UpdateState();

  // Calculates whether the renderer is available to control the browser
  // controls.
  content::BrowserControlsState CalculateStateForReasonRendererAvailability();

  // Calculates the value of the ControlsVisibilityReason::kOther state.
  content::BrowserControlsState CalculateStateForReasonOther();

  bool IsRendererHungOrCrashed();

  BrowserControlsNavigationStateHandlerDelegate* delegate_;

  // The controls are forced visible when a navigation starts, and allowed to
  // hide a short amount of time after done.
  bool force_show_during_load_ = false;

  // Timer used to set |force_show_during_load_| to false.
  base::OneShotTimer forced_show_during_load_timer_;

  // Last values supplied to the delegate.
  content::BrowserControlsState last_renderer_availability_state_ =
      content::BROWSER_CONTROLS_STATE_BOTH;
  content::BrowserControlsState last_other_state_ =
      content::BROWSER_CONTROLS_STATE_BOTH;

  // This is cached as WebContents::IsCrashed() does not always return the
  // right thing.
  bool is_crashed_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_H_
