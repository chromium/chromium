// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/page_load_metrics_initialize.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "weblayer/browser/heavy_ad_service_factory.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_utils.h"
#include "weblayer/browser/page_load_metrics_observer_impl.h"
#include "weblayer/browser/weblayer_page_load_metrics_memory_tracker_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace page_load_metrics {
class PageLoadMetricsMemoryTracker;
}  // namespace page_load_metrics

namespace weblayer {

namespace {

base::RepeatingCallback<void(page_load_metrics::PageLoadTracker*)>*
    g_callback_for_testing;

class PageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderBase {
 public:
  PageLoadMetricsEmbedder(const PageLoadMetricsEmbedder&) = delete;
  PageLoadMetricsEmbedder& operator=(const PageLoadMetricsEmbedder&) = delete;
  explicit PageLoadMetricsEmbedder(content::WebContents* web_contents)
      : PageLoadMetricsEmbedderBase(web_contents) {}
  ~PageLoadMetricsEmbedder() override = default;

  // page_load_metrics::PageLoadMetricsEmbedderBase:
  bool IsNewTabPageUrl(const GURL& url) override { return false; }
  bool IsNoStatePrefetch(content::WebContents* web_contents) override {
    return NoStatePrefetchContentsFromWebContents(web_contents);
  }
  bool IsExtensionUrl(const GURL& url) override { return false; }
  bool IsSidePanel(content::WebContents* web_contents) override {
    // The side panel is not supported in WebLayer so this always returns false.
    return false;
  }
  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override {
    if (!base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring))
      return nullptr;

    return WeblayerPageLoadMetricsMemoryTrackerFactory::GetForBrowserContext(
        browser_context);
  }

 protected:
  // page_load_metrics::PageLoadMetricsEmbedderBase:
  void RegisterEmbedderObservers(
      page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<PageLoadMetricsObserverImpl>());

    if (!IsNoStatePrefetch(web_contents())) {
      std::unique_ptr<page_load_metrics::AdsPageLoadMetricsObserver>
          ads_observer =
              page_load_metrics::AdsPageLoadMetricsObserver::CreateIfNeeded(
                  tracker->GetWebContents(),
                  HeavyAdServiceFactory::GetForBrowserContext(
                      tracker->GetWebContents()->GetBrowserContext()),
                  base::BindRepeating(&i18n::GetApplicationLocale));
      if (ads_observer)
        tracker->AddObserver(std::move(ads_observer));
    }

    if (g_callback_for_testing)
      (*g_callback_for_testing).Run(tracker);
  }
};

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  // Change this method? consider to modify the peer in
  // android_webview/browser/page_load_metrics/page_load_metrics_initialize.cc
  // chrome/browser/page_load_metrics/page_load_metrics_initialize.cc
  // as well.
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, std::make_unique<PageLoadMetricsEmbedder>(web_contents));
}

void SetRegisterEmbedderObserversForTesting(
    base::RepeatingCallback<void(page_load_metrics::PageLoadTracker*)>*
        callback) {
  g_callback_for_testing = callback;
}

}  // namespace weblayer
