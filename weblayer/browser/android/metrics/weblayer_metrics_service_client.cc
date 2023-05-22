// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"

#include <jni.h>
#include <cstdint>
#include <memory>

#include "base/base64.h"
#include "base/no_destructor.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/google_api_keys.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_fragment_list.h"
#include "weblayer/browser/browser_process.h"
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

  PageLoadMetricsProvider(const PageLoadMetricsProvider&) = delete;
  PageLoadMetricsProvider& operator=(const PageLoadMetricsProvider&) = delete;

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
  BrowserFragmentList::GetInstance()->AddObserver(this);
}

WebLayerMetricsServiceClient::~WebLayerMetricsServiceClient() {
  BrowserFragmentList::GetInstance()->RemoveObserver(this);
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

  GetMetricsService()->GetSyntheticTrialRegistry()->RegisterExternalExperiments(
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

const network_time::NetworkTimeTracker*
WebLayerMetricsServiceClient::GetNetworkTimeTracker() {
  return BrowserProcess::GetInstance()->GetNetworkTimeTracker();
}

int WebLayerMetricsServiceClient::GetSampleRatePerMille() const {
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
}

void WebLayerMetricsServiceClient::OnMetricsNotStarted() {
  post_start_tasks_.clear();
}

int WebLayerMetricsServiceClient::GetPackageNameLimitRatePerMille() {
  return kPackageNameLimitRatePerMille;
}

void WebLayerMetricsServiceClient::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {
  service->RegisterMetricsProvider(std::make_unique<PageLoadMetricsProvider>());
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

void WebLayerMetricsServiceClient::ApplyConsent(bool user_consent,
                                                bool app_consent) {
  // TODO(https://crbug.com/1155163): update this once consent can be
  // dynamically changed.
  // It is expected this function is called only once, and that prior to this
  // function the metrics service has not been started. The reason the metric
  // service should not have been started prior to this function is that the
  // metrics service is only started if consent is given, and this function is
  // responsible for setting consent.
  DCHECK(!GetMetricsServiceIfStarted());
  // UkmService is only created if consent is given.
  DCHECK(!GetUkmService());

  SetHaveMetricsConsent(user_consent, app_consent);
  ApplyForegroundStateToServices();
}

void WebLayerMetricsServiceClient::ApplyForegroundStateToServices() {
  const bool is_in_foreground =
      BrowserFragmentList::GetInstance()->HasAtLeastOneResumedBrowser();
  if (auto* metrics_service = WebLayerMetricsServiceClient::GetInstance()
                                  ->GetMetricsServiceIfStarted()) {
    if (is_in_foreground)
      metrics_service->OnAppEnterForeground();
    else
      metrics_service->OnAppEnterBackground();
  }

  if (auto* ukm_service = GetUkmService()) {
    if (is_in_foreground)
      ukm_service->OnAppEnterForeground();
    else
      ukm_service->OnAppEnterBackground();
  }
}

void WebLayerMetricsServiceClient::ProfileCreated(ProfileImpl* profile) {
  UpdateUkmService();
}

void WebLayerMetricsServiceClient::ProfileDestroyed(ProfileImpl* profile) {
  UpdateUkmService();
}

void WebLayerMetricsServiceClient::
    OnHasAtLeastOneResumedBrowserFragmentStateChanged(bool new_value) {
  ApplyForegroundStateToServices();
}

void JNI_ApplyConsentHelper(bool user_consent, bool app_consent) {
  WebLayerMetricsServiceClient::GetInstance()->ApplyConsent(user_consent,
                                                            app_consent);
}

// static
void JNI_MetricsServiceClient_SetHaveMetricsConsent(JNIEnv* env,
                                                    jboolean user_consent,
                                                    jboolean app_consent) {
  JNI_ApplyConsentHelper(user_consent, app_consent);
}

}  // namespace weblayer
