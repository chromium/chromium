// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PAGE_LOAD_METRICS_OBSERVER_IMPL_H_
#define WEBLAYER_BROWSER_PAGE_LOAD_METRICS_OBSERVER_IMPL_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace weblayer {

class PageLoadMetricsObserverImpl
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PageLoadMetricsObserverImpl() = default;
  ~PageLoadMetricsObserverImpl() override = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void ReportBufferedMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

 private:
  bool reported_buffered_metrics_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PAGE_LOAD_METRICS_OBSERVER_IMPL_H_
