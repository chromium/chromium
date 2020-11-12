// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"

#include <jni.h>
#include <cstdint>
#include <memory>

#include "base/base64.h"
#include "base/no_destructor.h"
#include "components/metrics/content/content_stability_metrics_provider.h"
#include "components/metrics/content/extensions_helper.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/google_api_keys.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/java/jni/MetricsServiceClient_jni.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

namespace {

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 10%, which is the same as chrome.
const int kStableSampledInRatePerMille = 100;

// Sample non-stable channels at 99%, to boost volume for pre-stable
// experiments. We choose 99% instead of 100% for consistency with Chrome and to
// exercise the out-of-sample code path.
const int kBetaDevCanarySampledInRatePerMille = 990;

// As a mitigation to preserve user privacy, the privacy team has asked that we
// upload package name with no more than 10% of UMA records. This is to mitigate
// fingerprinting for users on low-usage applications (if an app only has a
// a small handful of users, there's a very good chance many of them won't be
// uploading UMA records due to sampling). Do not change this constant without
// consulting with the privacy team.
const int kPackageNameLimitRatePerMille = 100;

// MetricsProvider that interfaces with page_load_metrics.
class PageLoadMetricsProvider : public metrics::MetricsProvider {
 public:
  PageLoadMetricsProvider() = default;
  ~PageLoadMetricsProvider() override = default;

  // metrics:MetricsProvider implementation:
  void OnAppEnterBackground() override {
    auto tabs = TabImpl::GetAllTabImpl();
    for (auto* tab : tabs) {
      page_load_metrics::MetricsWebContentsObserver* observer =
          page_load_metrics::MetricsWebContentsObserver::FromWebContents(
              tab->web_contents());
      if (observer)
        observer->FlushMetricsOnAppEnterBackground();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsProvider);
};

}  // namespace

// static
WebLayerMetricsServiceClient* WebLayerMetricsServiceClient::GetInstance() {
  static base::NoDestructor<WebLayerMetricsServiceClient> client;
  client->EnsureOnValidSequence();
  return client.get();
}

WebLayerMetricsServiceClient::WebLayerMetricsServiceClient() {
  ProfileImpl::AddProfileObserver(this);
}

WebLayerMetricsServiceClient::~WebLayerMetricsServiceClient() {
  ProfileImpl::RemoveProfileObserver(this);
}

void WebLayerMetricsServiceClient::RegisterExternalExperiments(
    const std::vector<int>& experiment_ids) {
  if (!GetMetricsService()) {
    if (!IsConsentDetermined()) {
      post_start_tasks_.push_back(base::BindOnce(
          &WebLayerMetricsServiceClient::RegisterExternalExperiments,
          base::Unretained(this), experiment_ids));
    }
    return;
  }

  GetMetricsService()->synthetic_trial_registry()->RegisterExternalExperiments(
      "WebLayerExperiments", experiment_ids,
      variations::SyntheticTrialRegistry::kOverrideExistingIds);
}

int32_t WebLayerMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::ANDROID_WEBLAYER;
}

bool WebLayerMetricsServiceClient::IsExternalExperimentAllowlistEnabled() {
  // RegisterExternalExperiments() is actually used to register experiment ids
  // coming from the app embedding WebLayer itself, rather than externally. So
  // the allowlist shouldn't be applied.
  return false;
}

bool WebLayerMetricsServiceClient::IsUkmAllowedForAllProfiles() {
  for (auto* profile : ProfileImpl::GetAllProfiles()) {
    if (!profile->GetBooleanSetting(SettingType::UKM_ENABLED))
      return false;
  }
  return true;
}

std::string WebLayerMetricsServiceClient::GetUploadSigningKey() {
  std::string decoded_key;
  base::Base64Decode(google_apis::GetMetricsKey(), &decoded_key);
  return decoded_key;
}

int WebLayerMetricsServiceClient::GetSampleRatePerMille() {
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    return kStableSampledInRatePerMille;
  }
  return kBetaDevCanarySampledInRatePerMille;
}

void WebLayerMetricsServiceClient::OnMetricsStart() {
  for (auto& task : post_start_tasks_) {
    std::move(task).Run();
  }
  post_start_tasks_.clear();
  GetMetricsService()->synthetic_trial_registry()->AddSyntheticTrialObserver(
      variations::VariationsIdsProvider::GetInstance());
}

void WebLayerMetricsServiceClient::OnMetricsNotStarted() {
  post_start_tasks_.clear();
}

int WebLayerMetricsServiceClient::GetPackageNameLimitRatePerMille() {
  return kPackageNameLimitRatePerMille;
}

void WebLayerMetricsServiceClient::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {
  service->RegisterMetricsProvider(
      std::make_unique<metrics::ContentStabilityMetricsProvider>(pref_service(),
                                                                 nullptr));
  service->RegisterMetricsProvider(std::make_unique<PageLoadMetricsProvider>());
}

bool WebLayerMetricsServiceClient::IsPersistentHistogramsEnabled() {
  return true;
}

bool WebLayerMetricsServiceClient::IsOffTheRecordSessionActive() {
  for (auto* profile : ProfileImpl::GetAllProfiles()) {
    if (profile->GetBrowserContext()->IsOffTheRecord())
      return true;
  }

  return false;
}

scoped_refptr<network::SharedURLLoaderFactory>
WebLayerMetricsServiceClient::GetURLLoaderFactory() {
  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

void WebLayerMetricsServiceClient::ProfileCreated(ProfileImpl* profile) {
  UpdateUkmService();
}

void WebLayerMetricsServiceClient::ProfileDestroyed(ProfileImpl* profile) {
  UpdateUkmService();
}

// static
void JNI_MetricsServiceClient_SetHaveMetricsConsent(JNIEnv* env,
                                                    jboolean user_consent,
                                                    jboolean app_consent) {
  WebLayerMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(
      user_consent, app_consent);
}

}  // namespace weblayer
