// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/privacy_budget/clock_skew/clock_skew_tool.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_tracker.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace clock_skew {

ClockSkewTool::ClockSkewTool() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("clock_skew_tool");

  features_.InitAndEnableFeatureWithParameters(
      network_time::kNetworkTimeServiceQuerying,
      {// Guarantee that the `NetworkTimeTracker::ShouldIssueTimeQuery` will
       // not choose to rate-limit our requested queries.
       {"RandomQueryProbability", "1.0"},
       {"FetchBehavior", "background-and-on-demand"},
       {"CheckTimeInterval", "10s"},
       {"BackoffInterval", "10s"},
       {"ClockDriftSamples", "2"},
       {"ClockDriftSamplesDistance", "2s"}});

  network_time::NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

  // Initialize the network service.
  mojo::core::Init();

  mojo::Remote<network::mojom::NetworkService> network_service_remote;
  network_service_ = network::NetworkService::Create(
      network_service_remote.BindNewPipeAndPassReceiver());

  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  network_context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  network_context_params->enable_brotli = true;

  mojo::Remote<network::mojom::NetworkContext> network_context_remote;
  network_context_ = std::make_unique<network::NetworkContext>(
      network_service_.get(),
      network_context_remote.BindNewPipeAndPassReceiver(),
      std::move(network_context_params));

  network::mojom::URLLoaderFactoryParamsPtr url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;

  network_context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(),
      std::move(url_loader_factory_params));

  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          url_loader_factory_.get());

  tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
      std::make_unique<base::DefaultClock>(),
      std::make_unique<base::DefaultTickClock>(), &pref_service_,
      shared_url_loader_factory_);

  CHECK(tracker_->AreTimeFetchesEnabled());
  CHECK_EQ(
      tracker_->GetFetchBehavior(),
      network_time::NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
}

ClockSkewTool::~ClockSkewTool() = default;

}  // namespace clock_skew
