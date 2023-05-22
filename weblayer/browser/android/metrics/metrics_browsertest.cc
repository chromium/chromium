// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/stability_metrics_helper.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "weblayer/browser/android/metrics/metrics_test_helper.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/browser_fragment_list.h"
#include "weblayer/browser/browser_list.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/profile.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

bool HasHistogramWithHash(const metrics::ChromeUserMetricsExtension& uma_log,
                          uint64_t hash) {
  for (int i = 0; i < uma_log.histogram_event_size(); ++i) {
    if (uma_log.histogram_event(i).name_hash() == hash) {
      return true;
    }
  }
  return false;
}

}  // namespace

class MetricsBrowserTest : public WebLayerBrowserTest {
 public:
  void SetUp() override {
    metrics::ForceEnableMetricsReportingForTesting();

    InstallTestGmsBridge(GetConsentType(),
                         base::BindRepeating(&MetricsBrowserTest::OnLogMetrics,
                                             base::Unretained(this)));
    WebLayerMetricsServiceClient::GetInstance()->SetFastStartupForTesting(true);
    WebLayerMetricsServiceClient::GetInstance()->SetUploadIntervalForTesting(
        base::Milliseconds(10));
    WebLayerBrowserTest::SetUp();
  }

  void TearDown() override {
    RemoveTestGmsBridge();
    WebLayerBrowserTest::TearDown();
  }

  void OnLogMetrics(metrics::ChromeUserMetricsExtension metric) {
    metrics_logs_.push_back(metric);
    if (on_new_log_)
      std::move(on_new_log_).Run();
  }

  metrics::ChromeUserMetricsExtension WaitForNextMetricsLog() {
    if (metrics_logs_.empty()) {
      base::RunLoop run_loop;
      on_new_log_ = run_loop.QuitClosure();
      run_loop.Run();
      DCHECK(!metrics_logs_.empty());
    }
    metrics::ChromeUserMetricsExtension tmp = std::move(metrics_logs_.front());
    metrics_logs_.pop_front();
    return tmp;
  }

  size_t GetNumLogs() const { return metrics_logs_.size(); }

  virtual ConsentType GetConsentType() { return ConsentType::kConsent; }

 private:
  std::unique_ptr<Profile> profile_;
  std::deque<metrics::ChromeUserMetricsExtension> metrics_logs_;
  base::OnceClosure on_new_log_;
};

IN_PROC_BROWSER_TEST_F(MetricsBrowserTest, ProtoHasExpectedFields) {
  metrics::ChromeUserMetricsExtension log = WaitForNextMetricsLog();
  EXPECT_EQ(metrics::ChromeUserMetricsExtension::ANDROID_WEBLAYER,
            log.product());
  EXPECT_TRUE(log.has_client_id());
  EXPECT_TRUE(log.has_session_id());

  const metrics::SystemProfileProto& system_profile = log.system_profile();
  EXPECT_TRUE(system_profile.has_build_timestamp());
  EXPECT_TRUE(system_profile.has_app_version());
  EXPECT_TRUE(system_profile.has_channel());
  EXPECT_TRUE(system_profile.has_install_date());
  EXPECT_TRUE(system_profile.has_application_locale());
  EXPECT_TRUE(system_profile.has_low_entropy_source());
  EXPECT_TRUE(system_profile.has_old_low_entropy_source());
  EXPECT_EQ("Android", system_profile.os().name());
  EXPECT_TRUE(system_profile.os().has_version());
  EXPECT_TRUE(system_profile.hardware().has_system_ram_mb());
  EXPECT_TRUE(system_profile.hardware().has_hardware_class());
  EXPECT_TRUE(system_profile.hardware().has_screen_count());
  EXPECT_TRUE(system_profile.hardware().has_primary_screen_width());
  EXPECT_TRUE(system_profile.hardware().has_primary_screen_height());
  EXPECT_TRUE(system_profile.hardware().has_primary_screen_scale_factor());
  EXPECT_TRUE(system_profile.hardware().has_cpu_architecture());
  EXPECT_TRUE(system_profile.hardware().cpu().has_vendor_name());
  EXPECT_TRUE(system_profile.hardware().cpu().has_signature());
  EXPECT_TRUE(system_profile.hardware().cpu().has_num_cores());
  EXPECT_TRUE(system_profile.hardware().cpu().has_is_hypervisor());
  EXPECT_TRUE(system_profile.hardware().gpu().has_driver_version());
  EXPECT_TRUE(system_profile.hardware().gpu().has_gl_vendor());
  EXPECT_TRUE(system_profile.hardware().gpu().has_gl_renderer());
}

IN_PROC_BROWSER_TEST_F(MetricsBrowserTest, PageLoadsEnableMultipleUploads) {
  WaitForNextMetricsLog();

  // At this point, the MetricsService should be asleep, and should not have
  // created any more metrics logs.
  ASSERT_EQ(0u, GetNumLogs());

  // The start of a page load should be enough to indicate to the MetricsService
  // that the app is "in use" and it's OK to upload the next record. No need to
  // wait for onPageFinished, since this behavior should be gated on
  // NOTIFICATION_LOAD_START.
  shell()->tab()->GetNavigationController()->Navigate(GURL("about:blank"));

  // This may take slightly longer than UPLOAD_INTERVAL_MS, due to the time
  // spent processing the metrics log, but should be well within the timeout
  // (unless something is broken).
  WaitForNextMetricsLog();

  // If we get here, we got a second metrics log (and the test may pass). If
  // there was no second metrics log, then the above call will check fail with a
  // timeout. We should not assert the logs are empty however, because it's
  // possible we got a metrics log between onPageStarted & onPageFinished, in
  // which case onPageFinished would *also* wake up the metrics service, and we
  // might potentially have a third metrics log in the queue.
}

IN_PROC_BROWSER_TEST_F(MetricsBrowserTest, NavigationIncrementsPageLoadCount) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  metrics::ChromeUserMetricsExtension log = WaitForNextMetricsLog();
  // The initial log should not have a page load count (because nothing was
  // loaded).
  {
    const metrics::SystemProfileProto& system_profile = log.system_profile();
    ASSERT_TRUE(system_profile.has_stability());
    EXPECT_EQ(0, system_profile.stability().page_load_count());
    histogram_tester.ExpectBucketCount(
        "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 0);
  }

  // Loading a page should increment the page load count.
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page.html"), shell());
  log = WaitForNextMetricsLog();
  {
    const metrics::SystemProfileProto& system_profile = log.system_profile();
    ASSERT_TRUE(system_profile.has_stability());
    EXPECT_EQ(1, system_profile.stability().page_load_count());
    histogram_tester.ExpectBucketCount(
        "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 1);
  }
}

IN_PROC_BROWSER_TEST_F(MetricsBrowserTest, RendererHistograms) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page.html"), shell());

  uint64_t hash = base::HashMetricName("Android.SeccompStatus.RendererSandbox");

  bool collect_final_metrics_for_log_called = false;

  WebLayerMetricsServiceClient::GetInstance()
      ->SetCollectFinalMetricsForLogClosureForTesting(
          base::BindLambdaForTesting(
              [&]() { collect_final_metrics_for_log_called = true; }));

  // Not every WaitForNextMetricsLog call will end up calling
  // MetricsServiceClient::CollectFinalMetricsForLog since there may already be
  // staged logs to send (see ReportingService::SendNextLog). Since we need to
  // wait for CollectFinalMetricsForLog to be run after the navigate call above,
  // keep calling WaitForNextMetricsLog until CollectFinalMetricsForLog is
  // called.
  metrics::ChromeUserMetricsExtension uma_log;
  while (!collect_final_metrics_for_log_called)
    uma_log = WaitForNextMetricsLog();

  ASSERT_TRUE(HasHistogramWithHash(uma_log, hash));
}

class MetricsBrowserTestWithUserOptOut : public MetricsBrowserTest {
  ConsentType GetConsentType() override { return ConsentType::kNoConsent; }
};

IN_PROC_BROWSER_TEST_F(MetricsBrowserTestWithUserOptOut, MetricsNotRecorded) {
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, GetNumLogs());
}

class MetricsBrowserTestWithConfigurableConsent : public MetricsBrowserTest {
  ConsentType GetConsentType() override { return ConsentType::kDelayConsent; }
};

IN_PROC_BROWSER_TEST_F(MetricsBrowserTestWithConfigurableConsent,
                       IsInForegroundWhenConsentGiven) {
  // There should be at least one browser which is resumed. This is the trigger
  // for whether the MetricsService is considered in the foreground.
  EXPECT_TRUE(
      BrowserFragmentList::GetInstance()->HasAtLeastOneResumedBrowser());
  RunConsentCallback(true);
  // RunConsentCallback() should trigger the MetricsService to start.
  EXPECT_TRUE(WebLayerMetricsServiceClient::GetInstance()
                  ->GetMetricsServiceIfStarted());
  EXPECT_TRUE(WebLayerMetricsServiceClient::GetInstance()
                  ->GetMetricsService()
                  ->IsInForegroundForTesting());
}

}  // namespace weblayer
