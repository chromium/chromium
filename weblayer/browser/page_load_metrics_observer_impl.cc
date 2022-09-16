// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/page_load_metrics_observer_impl.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_utils.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserverImpl::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is only interested in events for outer-most frame that are
  // forwarded by PageLoadTracker. So, this class doesn't need observer-level
  // forwarding.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserverImpl::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Currently, prerendering is not enabled for WebLayer.
  //
  // TODO(https://crbug.com/1267224): If support prerendering, add callbacks,
  // e.g. notification of activation_start.
  return STOP_OBSERVING;
}

PageLoadMetricsObserverImpl::ObservePolicy
PageLoadMetricsObserverImpl::OnCommit(
    content::NavigationHandle* navigation_handle) {
#if BUILDFLAG(IS_ANDROID)
  if (!ukm::UkmRecorder::Get())
    return CONTINUE_OBSERVING;

  // If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
  // populate it with top-level page-load metrics.
  prerender::NoStatePrefetchManager* const no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (!no_state_prefetch_manager)
    return CONTINUE_OBSERVING;
  prerender::RecordNoStatePrefetchMetrics(navigation_handle,
                                          GetDelegate().GetPageUkmSourceId(),
                                          no_state_prefetch_manager);
#endif
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserverImpl::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, WebLayer may be killed without further
  // notification, so we record final metrics collected up to this point.
  ReportBufferedMetrics(timing);

  // We continue observing after being backgrounded, in case we are foregrounded
  // again without being killed. In those cases we may still report non-buffered
  // metrics such as FCP after being re-foregrounded.
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserverImpl::ObservePolicy
PageLoadMetricsObserverImpl::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ReportBufferedMetrics(timing);
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsObserverImpl::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ReportBufferedMetrics(timing);
}

void PageLoadMetricsObserverImpl::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  auto* tab = TabImpl::FromWebContents(GetDelegate().GetWebContents());
  if (!tab)
    return;

  auto* nav_controller =
      static_cast<NavigationControllerImpl*>(tab->GetNavigationController());
  nav_controller->OnFirstContentfulPaint(
      GetDelegate().GetNavigationStart(),
      *timing.paint_timing->first_contentful_paint);
}

void PageLoadMetricsObserverImpl::ReportBufferedMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This method may be invoked multiple times. Make sure that if we already
  // reported, we do not report again.
  if (reported_buffered_metrics_)
    return;
  reported_buffered_metrics_ = true;

  // Buffered metrics aren't available until after the navigation commits.
  if (!GetDelegate().DidCommit())
    return;

  auto* tab = TabImpl::FromWebContents(GetDelegate().GetWebContents());
  if (!tab)
    return;

  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (!largest_contentful_paint.ContainsValidTime())
    return;

  auto* nav_controller =
      static_cast<NavigationControllerImpl*>(tab->GetNavigationController());
  nav_controller->OnLargestContentfulPaint(GetDelegate().GetNavigationStart(),
                                           *largest_contentful_paint.Time());
}

}  // namespace weblayer
