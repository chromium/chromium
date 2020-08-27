// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_
#define WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {

class WebLayerMetricsServiceClient
    : public ::metrics::AndroidMetricsServiceClient,
      public ProfileImpl::ProfileObserver {
  friend class base::NoDestructor<WebLayerMetricsServiceClient>;

 public:
  static WebLayerMetricsServiceClient* GetInstance();

  WebLayerMetricsServiceClient();
  ~WebLayerMetricsServiceClient() override;

  void RegisterExternalExperiments(const std::vector<int>& experiment_ids);

  // metrics::MetricsServiceClient
  int32_t GetProduct() override;
  bool IsExternalExperimentAllowlistEnabled() override;
  bool IsUkmAllowedForAllProfiles() override;
  std::string GetUploadSigningKey() override;

  // metrics::AndroidMetricsServiceClient:
  int GetSampleRatePerMille() override;
  void OnMetricsStart() override;
  void OnMetricsNotStarted() override;
  int GetPackageNameLimitRatePerMille() override;
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override;
  bool IsPersistentHistogramsEnabled() override;
  bool IsOffTheRecordSessionActive() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  // ProfileImpl::ProfileObserver:
  void ProfileCreated(ProfileImpl* profile) override;
  void ProfileDestroyed(ProfileImpl* profile) override;

  std::vector<base::OnceClosure> post_start_tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerMetricsServiceClient);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_
