// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/page_load_metrics_initialize.h"

#include "base/macros.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "weblayer/browser/no_state_prefetch/prerender_utils.h"
#include "weblayer/browser/page_load_metrics_observer_impl.h"

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
  bool IsPrerender(content::WebContents* web_contents) override {
    return PrerenderContentsFromWebContents(web_contents);
  }
  bool IsExtensionUrl(const GURL& url) override { return false; }

 protected:
  // page_load_metrics::PageLoadMetricsEmbedderBase:
  void RegisterEmbedderObservers(
      page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<PageLoadMetricsObserverImpl>());

    if (g_callback_for_testing)
      (*g_callback_for_testing).Run(tracker);
  }
  bool IsPrerendering() const override {
    return PrerenderContentsFromWebContents(web_contents());
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
