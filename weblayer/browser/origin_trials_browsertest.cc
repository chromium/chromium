// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

// See
// https://chromium.googlesource.com/chromium/src/+/main/docs/origin_trials_integration.md
const char kTestTokenPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

const char kTrialEnabledDomain[] = "example.com";
const char kTrialEnabledPath[] = "/origin-trial";
const char kFrobulatePersistentTrialName[] = "FrobulatePersistent";
// Generated with
// tools/origin_trials/generate_token.py https://example.com \
//     FrobulatePersistent --expire-timestamp=2000000000
const char kFrobulatePersistentToken[] =
    "AzZfd1vKZ0SSGRGk/"
    "8nIszQSlHYjbuYVE3jwaNZG3X4t11zRhzPWWJwTZ+JJDS3JJsyEZcpz+y20pAP6/"
    "6upOQ4AAABdeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI"
    "6ICJGcm9idWxhdGVQZXJzaXN0ZW50IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

}  // namespace

class OriginTrialsBrowserTest : public WebLayerBrowserTest {
 public:
  OriginTrialsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kPersistentOriginTrials);
  }

  ~OriginTrialsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&OriginTrialsBrowserTest::InterceptRequest));
  }

  void TearDownOnMainThread() override {
    // Clean up any saved settings after test run
    GetBrowserContext()
        ->GetOriginTrialsControllerDelegate()
        ->ClearPersistedTokens();

    url_loader_interceptor_.reset();

    WebLayerBrowserTest::TearDownOnMainThread();
  }

  base::flat_set<std::string> GetPersistedTrials() {
    url::Origin origin = url::Origin::CreateFromNormalizedTuple(
        "https", kTrialEnabledDomain, 443);
    return GetBrowserContext()
        ->GetOriginTrialsControllerDelegate()
        ->GetPersistedTrialsForOrigin(origin, /*partition_origin*/ origin,
                                      base::Time::Now());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kTestTokenPublicKey);
  }

  // Navigate to an insecure domain
  void RequestToHttpDomain() {
    NavigateAndWaitForCompletion(GURL("http://127.0.0.1/"), shell());
  }

  // Navigate to our enabled origin without any Origin-Trial response headers
  void RequestWithoutHeaders() {
    GURL url(base::StrCat({"https://", kTrialEnabledDomain, "/"}));
    NavigateAndWaitForCompletion(url, shell());
  }

  // Navigate to our enabled origin with a response containing |token|
  // in the Origin-Trial header
  void RequestForOriginTrial() {
    GURL url(
        base::StrCat({"https://", kTrialEnabledDomain, kTrialEnabledPath}));
    NavigateAndWaitForCompletion(url, shell());
  }

  static bool InterceptRequest(
      content::URLLoaderInterceptor::RequestParams* params) {
    std::string headers =
        "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
    // Find the appropriate origin trial token.
    if (params->url_request.url.DomainIs(kTrialEnabledDomain) &&
        params->url_request.url.path() == kTrialEnabledPath) {
      // Construct and send the response.

      base::StrAppend(&headers,
                      {"Origin-Trial: ", kFrobulatePersistentToken, "\n"});
    }
    headers += '\n';
    std::string body = "<!DOCTYPE html><body>Hello world!</body>";
    content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                 params->client.get());
    return true;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, NoHeaderDoesNotEnableResponse) {
  RequestWithoutHeaders();
  base::flat_set<std::string> trials = GetPersistedTrials();
  EXPECT_TRUE(trials.empty());
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, ResponseEnablesOriginTrial) {
  RequestForOriginTrial();
  base::flat_set<std::string> trials = GetPersistedTrials();
  ASSERT_FALSE(trials.empty());
  EXPECT_EQ(kFrobulatePersistentTrialName, *(trials.begin()));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       TrialEnabledAfterNavigationToOtherDomain) {
  // Navigate to a page that enables a persistent origin trial
  RequestForOriginTrial();
  EXPECT_FALSE(GetPersistedTrials().empty());
  // Navigate to a different domain
  RequestToHttpDomain();

  // The trial should still be enabled
  base::flat_set<std::string> trials = GetPersistedTrials();
  ASSERT_FALSE(trials.empty());
  EXPECT_EQ(kFrobulatePersistentTrialName, *(trials.begin()));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       TrialDisabledAfterNavigationToSameDomain) {
  // Navigate to a page that enables a persistent origin trial
  RequestForOriginTrial();
  EXPECT_FALSE(GetPersistedTrials().empty());
  // Navigate to same domain without the Origin-Trial header set
  RequestWithoutHeaders();

  // The trial should no longer be enabled
  EXPECT_TRUE(GetPersistedTrials().empty());
}

}  // namespace weblayer
