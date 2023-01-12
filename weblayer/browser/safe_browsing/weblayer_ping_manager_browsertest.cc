// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/test_safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/weblayer_ping_manager_factory.h"
#include "weblayer/browser/safe_browsing/weblayer_user_population_helper.h"
#include "weblayer/test/weblayer_browser_test.h"

using safe_browsing::ClientSafeBrowsingReportRequest;
using ReportThreatDetailsResult =
    safe_browsing::PingManager::ReportThreatDetailsResult;

namespace weblayer {

class WeblayerPingManagerTest : public WebLayerBrowserTest {
 public:
  WeblayerPingManagerTest() {
    feature_list_.InitWithFeatures(
        {safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests,
         safe_browsing::kSafeBrowsingCsbrrWithToken},
        {});
  }

 protected:
  void RunReportThreatDetailsTest(bool is_enhanced_protection,
                                  bool is_signed_in,
                                  bool expect_access_token,
                                  bool expect_cookies_removed);

  base::test::ScopedFeatureList feature_list_;
  bool is_csbrr_token_feature_enabled_ = true;
  bool is_remove_cookies_feature_enabled_ = true;

 private:
  safe_browsing::TestSafeBrowsingTokenFetcher* SetUpTokenFetcher(
      safe_browsing::PingManager* ping_manager);
};
class RemoveCookiesFeatureDisabledWeblayerPingManagerTest
    : public WeblayerPingManagerTest {
 public:
  RemoveCookiesFeatureDisabledWeblayerPingManagerTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {safe_browsing::kSafeBrowsingCsbrrWithToken},
        {safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests});
    is_remove_cookies_feature_enabled_ = false;
  }
};
class CsbrrTokenFeatureDisabledWeblayerPingManagerTest
    : public WeblayerPingManagerTest {
 public:
  CsbrrTokenFeatureDisabledWeblayerPingManagerTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests},
        {safe_browsing::kSafeBrowsingCsbrrWithToken});
    is_csbrr_token_feature_enabled_ = false;
  }
};
class IncognitoModeWeblayerPingManagerTest : public WeblayerPingManagerTest {
 public:
  IncognitoModeWeblayerPingManagerTest() { SetShellStartsInIncognitoMode(); }
};

safe_browsing::TestSafeBrowsingTokenFetcher*
WeblayerPingManagerTest::SetUpTokenFetcher(
    safe_browsing::PingManager* ping_manager) {
  auto token_fetcher =
      std::make_unique<safe_browsing::TestSafeBrowsingTokenFetcher>();
  auto* raw_token_fetcher = token_fetcher.get();
  ping_manager->SetTokenFetcherForTesting(std::move(token_fetcher));
  return raw_token_fetcher;
}

void WeblayerPingManagerTest::RunReportThreatDetailsTest(
    bool is_enhanced_protection,
    bool is_signed_in,
    bool expect_access_token,
    bool expect_cookies_removed) {
  base::RunLoop csbrr_logged_run_loop;
  base::HistogramTester histogram_tester;
  if (is_enhanced_protection) {
    SetSafeBrowsingState(GetProfile()->GetBrowserContext()->pref_service(),
                         safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  }
  if (is_signed_in) {
    WebLayerPingManagerFactory::GetInstance()->SignInAccountForTesting();
  }
  auto* ping_manager = WebLayerPingManagerFactory::GetForBrowserContext(
      GetProfile()->GetBrowserContext());
  auto* raw_token_fetcher = SetUpTokenFetcher(ping_manager);
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddListenerForTesting();
  safe_browsing::WebUIInfoSingleton::GetInstance()
      ->SetOnCSBRRLoggedCallbackForTesting(csbrr_logged_run_loop.QuitClosure());

  std::string access_token = "testing_access_token";
  std::string input_report_content;
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  // The report must be non-empty. The selected property to set is arbitrary.
  report->set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  EXPECT_TRUE(report->SerializeToString(&input_report_content));
  ClientSafeBrowsingReportRequest expected_report;
  expected_report.ParseFromString(input_report_content);
  *expected_report.mutable_population() =
      GetUserPopulationForBrowserContext(GetProfile()->GetBrowserContext());
  std::string expected_report_content;
  EXPECT_TRUE(expected_report.SerializeToString(&expected_report_content));
  EXPECT_NE(input_report_content, expected_report_content);

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), expected_report_content);
        std::string header_value;
        bool found_header = request.headers.GetHeader(
            net::HttpRequestHeaders::kAuthorization, &header_value);
        EXPECT_EQ(found_header, expect_access_token);
        if (expect_access_token) {
          EXPECT_EQ(header_value, "Bearer " + access_token);
        }
        EXPECT_EQ(request.credentials_mode,
                  expect_cookies_removed
                      ? network::mojom::CredentialsMode::kOmit
                      : network::mojom::CredentialsMode::kInclude);
        histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.ClientSafeBrowsingReport.RequestHasToken",
            /*sample=*/expect_access_token,
            /*expected_bucket_count=*/1);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ReportThreatDetailsResult result =
      ping_manager->ReportThreatDetails(std::move(report));
  EXPECT_EQ(result, ReportThreatDetailsResult::SUCCESS);
  EXPECT_EQ(raw_token_fetcher->WasStartCalled(), expect_access_token);
  if (expect_access_token) {
    raw_token_fetcher->RunAccessTokenCallback(access_token);
  }
  csbrr_logged_run_loop.Run();
  EXPECT_EQ(
      safe_browsing::WebUIInfoSingleton::GetInstance()->csbrrs_sent().size(),
      1u);
  safe_browsing::WebUIInfoSingleton::GetInstance()->ClearListenerForTesting();
}

IN_PROC_BROWSER_TEST_F(WeblayerPingManagerTest,
                       ReportThreatDetailsWithAccessToken) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*expect_access_token=*/true,
                             /*expect_cookies_removed=*/true);
}
IN_PROC_BROWSER_TEST_F(
    RemoveCookiesFeatureDisabledWeblayerPingManagerTest,
    ReportThreatDetailsWithAccessToken_RemoveCookiesFeatureDisabled) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*expect_access_token=*/true,
                             /*expect_cookies_removed=*/false);
}
IN_PROC_BROWSER_TEST_F(IncognitoModeWeblayerPingManagerTest,
                       ReportThreatDetailsWithoutAccessToken_Incognito) {
  EXPECT_EQ(WebLayerPingManagerFactory::GetForBrowserContext(
                GetProfile()->GetBrowserContext()),
            nullptr);
}
IN_PROC_BROWSER_TEST_F(
    WeblayerPingManagerTest,
    ReportThreatDetailsWithoutAccessToken_NotEnhancedProtection) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/false,
                             /*is_signed_in=*/true,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false);
}
IN_PROC_BROWSER_TEST_F(WeblayerPingManagerTest,
                       ReportThreatDetailsWithoutAccessToken_NotSignedIn) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/false,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false);
}
// TODO(crbug.com/1296615): remove test case,
// CsbrrTokenFeatureDisabledWeblayerPingManagerTest class, and
// is_csbrr_token_feature_enabled_ property when deprecating
// kSafeBrowsingCsbrrWithToken feature
IN_PROC_BROWSER_TEST_F(
    CsbrrTokenFeatureDisabledWeblayerPingManagerTest,
    ReportThreatDetailsWithoutAccessToken_CsbrrTokenFeatureDisabled) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false);
}

IN_PROC_BROWSER_TEST_F(WeblayerPingManagerTest, ReportSafeBrowsingHit) {
  safe_browsing::HitReport hit_report;
  hit_report.post_data = "testing_hit_report_post_data";
  // Threat type and source are arbitrary but specified so that determining the
  // URL does not does throw an error due to input validation.
  hit_report.threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  hit_report.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;

  auto* ping_manager = WebLayerPingManagerFactory::GetForBrowserContext(
      GetProfile()->GetBrowserContext());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), hit_report.post_data);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ping_manager->ReportSafeBrowsingHit(hit_report);
}

}  // namespace weblayer
