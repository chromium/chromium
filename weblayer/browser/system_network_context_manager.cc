// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/system_network_context_manager.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/network_service_instance.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace weblayer {

namespace {

// The global instance of the SystemNetworkContextmanager.
SystemNetworkContextManager* g_system_network_context_manager = nullptr;

}  // namespace

// static
SystemNetworkContextManager* SystemNetworkContextManager::CreateInstance(
    const std::string& user_agent) {
  DCHECK(!g_system_network_context_manager);
  g_system_network_context_manager =
      new SystemNetworkContextManager(user_agent);
  return g_system_network_context_manager;
}

// static
bool SystemNetworkContextManager::HasInstance() {
  return !!g_system_network_context_manager;
}

// static
SystemNetworkContextManager* SystemNetworkContextManager::GetInstance() {
  DCHECK(g_system_network_context_manager);
  return g_system_network_context_manager;
}

// static
void SystemNetworkContextManager::DeleteInstance() {
  DCHECK(g_system_network_context_manager);
  delete g_system_network_context_manager;
  g_system_network_context_manager = nullptr;
}

// static
network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateDefaultNetworkContextParams(
    const std::string& user_agent) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  network_context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  ConfigureDefaultNetworkContextParams(network_context_params.get(),
                                       user_agent);
  variations::UpdateCorsExemptHeaderForVariations(network_context_params.get());
  return network_context_params;
}

// static
void SystemNetworkContextManager::ConfigureDefaultNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params,
    const std::string& user_agent) {
  network_context_params->user_agent = user_agent;
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
  // We're not configuring the cookie encryption on these platforms yet.
  network_context_params->enable_encrypted_cookies = false;
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) ||
        // BUILDFLAG(IS_WIN)
}

SystemNetworkContextManager::SystemNetworkContextManager(
    const std::string& user_agent)
    : user_agent_(user_agent) {}

SystemNetworkContextManager::~SystemNetworkContextManager() = default;

network::mojom::NetworkContext*
SystemNetworkContextManager::GetSystemNetworkContext() {
  if (!system_network_context_ || !system_network_context_.is_connected()) {
    // This should call into OnNetworkServiceCreated(), which will re-create
    // the network service, if needed. There's a chance that it won't be
    // invoked, if the NetworkContext has encountered an error but the
    // NetworkService has not yet noticed its pipe was closed. In that case,
    // trying to create a new NetworkContext would fail, anyways, and hopefully
    // a new NetworkContext will be created on the next GetContext() call.
    content::GetNetworkService();
    DCHECK(system_network_context_);
  }

  return system_network_context_.get();
}

void SystemNetworkContextManager::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  system_network_context_.reset();
  network_service->CreateNetworkContext(
      system_network_context_.BindNewPipeAndPassReceiver(),
      CreateSystemNetworkContextManagerParams());
}

network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateSystemNetworkContextManagerParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      CreateDefaultNetworkContextParams(user_agent_);
  return network_context_params;
}

scoped_refptr<network::SharedURLLoaderFactory>
SystemNetworkContextManager::GetSharedURLLoaderFactory() {
  if (!url_loader_factory_) {
    auto url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_corb_enabled = false;
    GetSystemNetworkContext()->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }
  return shared_url_loader_factory_;
}

net_log::NetExportFileWriter*
SystemNetworkContextManager::GetNetExportFileWriter() {
  if (!net_export_file_writer_) {
    net_export_file_writer_ = std::make_unique<net_log::NetExportFileWriter>();
  }
  return net_export_file_writer_.get();
}

}  // namespace weblayer
