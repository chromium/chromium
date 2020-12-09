// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/page_load_metrics_observer_impl.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "components/no_state_prefetch/browser/prerender_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

PageLoadMetricsObserverImpl::ObservePolicy
PageLoadMetricsObserverImpl::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
#if defined(OS_ANDROID)
  if (!ukm::UkmRecorder::Get())
    return CONTINUE_OBSERVING;

  // If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
  // populate it with top-level page-load metrics.
  prerender::PrerenderManager* const prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (!prerender_manager)
    return CONTINUE_OBSERVING;
  prerender::RecordNoStatePrefetchMetrics(navigation_handle, source_id,
                                          prerender_manager);
#endif
  return CONTINUE_OBSERVING;
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

}  // namespace weblayer
