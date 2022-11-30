// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/client_side_detection_service_factory.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/features.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

using safe_browsing::ClientSideDetectionService;
using safe_browsing::ClientSideModel;
using safe_browsing::ClientSidePhishingModel;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::StrictMock;

class ClientSideDetectionServiceBrowserTest : public WebLayerBrowserTest {
 public:
  ClientSideDetectionServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kWebLayerClientSidePhishingDetection);
  }
  content::WebContents* GetWebContents() {
    return static_cast<TabImpl*>(shell()->tab())->web_contents();
  }

 private:
  void SetUpOnMainThread() override {
    NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  }
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1217128): Re-enable this test, once we have a more reliable
// method of ensuring the SetPhishingModel IPC comes before the
// StartPhishingDetection IPC.
IN_PROC_BROWSER_TEST_F(ClientSideDetectionServiceBrowserTest,
                       DISABLED_NewHostGetsModel) {
  PrefService* prefs = GetProfile()->GetBrowserContext()->pref_service();
  prefs->SetBoolean(::prefs::kSafeBrowsingEnabled, false);

  ClientSideModel model;
  model.set_max_words_per_term(0);
  std::string model_str;
  model.SerializeToString(&model_str);

  ClientSidePhishingModel::GetInstance()->SetModelStrForTesting(model_str);

  // Enable Safe Browsing and the CSD service.
  prefs->SetBoolean(::prefs::kSafeBrowsingEnabled, true);

  base::RunLoop run_loop;

  content::RenderFrameHost* rfh = GetWebContents()->GetPrimaryMainFrame();
  mojo::Remote<safe_browsing::mojom::PhishingDetector> phishing_detector;
  rfh->GetRemoteInterfaces()->GetInterface(
      phishing_detector.BindNewPipeAndPassReceiver());

  safe_browsing::mojom::PhishingDetectorResult result;
  std::string verdict;
  phishing_detector->StartPhishingDetection(
      GURL("about:blank"),
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             safe_browsing::mojom::PhishingDetectorResult* out_result,
             std::string* out_verdict,
             safe_browsing::mojom::PhishingDetectorResult result,
             const std::string& verdict) {
            *out_result = result;
            *out_verdict = verdict;
            quit_closure.Run();
          },
          run_loop.QuitClosure(), &result, &verdict));

  run_loop.Run();

  // The model classification will run, but will return an invalid score.
  EXPECT_EQ(result,
            safe_browsing::mojom::PhishingDetectorResult::INVALID_SCORE);
}

}  // namespace weblayer
