// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/ukm_page_load_metrics_observer.h"

#include "build/build_config.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "components/no_state_prefetch/browser/prerender_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"

namespace weblayer {

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
UkmPageLoadMetricsObserver::CreateIfNeeded() {
  if (!ukm::UkmRecorder::Get()) {
    return nullptr;
  }
  return std::make_unique<UkmPageLoadMetricsObserver>();
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
#if defined(OS_ANDROID)
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

}  // namespace weblayer
