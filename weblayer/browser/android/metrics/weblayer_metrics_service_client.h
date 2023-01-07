// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_
#define WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "weblayer/browser/browser_list_observer.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {

class WebLayerMetricsServiceClient
    : public ::metrics::AndroidMetricsServiceClient,
      public ProfileImpl::ProfileObserver,
      public BrowserListObserver {
  friend class base::NoDestructor<WebLayerMetricsServiceClient>;

 public:
  static WebLayerMetricsServiceClient* GetInstance();

  WebLayerMetricsServiceClient();

  WebLayerMetricsServiceClient(const WebLayerMetricsServiceClient&) = delete;
  WebLayerMetricsServiceClient& operator=(const WebLayerMetricsServiceClient&) =
      delete;

  ~WebLayerMetricsServiceClient() override;

  void RegisterExternalExperiments(const std::vector<int>& experiment_ids);

  // metrics::MetricsServiceClient
  int32_t GetProduct() override;
  bool IsExternalExperimentAllowlistEnabled() override;
  bool IsUkmAllowedForAllProfiles() override;
  std::string GetUploadSigningKey() override;

  // metrics::AndroidMetricsServiceClient:
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  int GetSampleRatePerMille() const override;
  void OnMetricsStart() override;
  void OnMetricsNotStarted() override;
  int GetPackageNameLimitRatePerMille() override;
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override;
  bool IsOffTheRecordSessionActive() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  friend void JNI_ApplyConsentHelper(bool user_consent, bool app_consent);

  // Called once when consent has been determined.
  void ApplyConsent(bool user_consent, bool app_consent);

  // Updates the services based on the foreground state.
  void ApplyForegroundStateToServices();

  // ProfileImpl::ProfileObserver:
  void ProfileCreated(ProfileImpl* profile) override;
  void ProfileDestroyed(ProfileImpl* profile) override;

  // BrowserListObserver:
  void OnHasAtLeastOneResumedBrowserStateChanged(bool new_value) override;

  std::vector<base::OnceClosure> post_start_tasks_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_
