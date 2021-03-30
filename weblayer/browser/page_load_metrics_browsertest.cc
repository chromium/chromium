// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/page_load_metrics_initialize.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

using PageLoadMetricsBrowserTest = WebLayerBrowserTest;

class PageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit PageLoadMetricsObserver(base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}
  ~PageLoadMetricsObserver() override = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override {
    on_first_paint_seen_ = true;
    QuitRunLoopIfReady();
  }

  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override {
    on_first_contentful_paint_seen_ = true;
    QuitRunLoopIfReady();
  }

 private:
  void QuitRunLoopIfReady() {
    if (on_first_paint_seen_ && on_first_contentful_paint_seen_)
      quit_closure_.Run();
  }

  bool on_first_paint_seen_ = false;
  bool on_first_contentful_paint_seen_ = false;
  base::RepeatingClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, Heartbeat) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  base::RunLoop run_loop;

  auto callback = base::BindLambdaForTesting(
      [&](page_load_metrics::PageLoadTracker* tracker) {
        tracker->AddObserver(
            std::make_unique<PageLoadMetricsObserver>(run_loop.QuitClosure()));

        // Don't need this anymore
        SetRegisterEmbedderObserversForTesting(nullptr);
      });
  SetRegisterEmbedderObserversForTesting(&callback);

  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page.html"), shell());

  run_loop.Run();

  // Look for prefix because on Android the name would be different if the tab
  // is not in foreground initially. This seems to happen on a slow test bot.
  EXPECT_GE(histogram_tester
                .GetTotalCountsForPrefix(
                    "PageLoad.PaintTiming.NavigationToFirstPaint")
                .size(),
            1u);
  EXPECT_GE(histogram_tester
                .GetTotalCountsForPrefix(
                    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint")
                .size(),
            1u);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, UserCounter) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/form.html"),
                               shell());
  NavigateAndWaitForCompletion(GURL("about:blank"), shell());  // Flush.

  histogram_tester.ExpectBucketCount("Blink.UseCounter.MainFrame.Features",
                                     blink::mojom::WebFeature::kPageVisits, 1);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                     blink::mojom::WebFeature::kFormElement, 1);
}

}  // namespace weblayer
