// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
#define WEBLAYER_BROWSER_SYSTEM_NETWORK_CONTEXT_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace net_log {
class NetExportFileWriter;
}

namespace weblayer {

// Manages a system-wide network context that's not tied to a profile.
class SystemNetworkContextManager {
 public:
  // Creates the global instance of SystemNetworkContextManager.
  static SystemNetworkContextManager* CreateInstance(
      const std::string& user_agent);

  // Checks if the global SystemNetworkContextManager has been created.
  static bool HasInstance();

  // Gets the global SystemNetworkContextManager instance or DCHECKs if there
  // isn't one..
  static SystemNetworkContextManager* GetInstance();

  // Destroys the global SystemNetworkContextManager instance.
  static void DeleteInstance();

  static network::mojom::NetworkContextParamsPtr
  CreateDefaultNetworkContextParams(const std::string& user_agent);

  static void ConfigureDefaultNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params,
      const std::string& user_agent);

  SystemNetworkContextManager(const SystemNetworkContextManager&) = delete;
  SystemNetworkContextManager& operator=(const SystemNetworkContextManager&) =
      delete;

  ~SystemNetworkContextManager();

  // Returns the System NetworkContext. Does any initialization of the
  // NetworkService that may be needed when first called.
  network::mojom::NetworkContext* GetSystemNetworkContext();

  // Called when content creates a NetworkService. Creates the
  // system NetworkContext, if the network service is enabled.
  void OnNetworkServiceCreated(network::mojom::NetworkService* network_service);

  // Returns a SharedURLLoaderFactory owned by the SystemNetworkContextManager
  // that is backed by the SystemNetworkContext.
  // NOTE: This factory assumes that the network service is running in the
  // browser process, which is a valid assumption for Android. If WebLayer is
  // productionized beyond Android, it will need to be extended to handle
  // network service crashes.
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory();

  // Returns a shared global NetExportFileWriter instance, used by net-export.
  // It lives here so it can outlive chrome://net-export/ if the tab is closed
  // or destroyed, and so that it's destroyed before Mojo is shut down.
  net_log::NetExportFileWriter* GetNetExportFileWriter();

 private:
  explicit SystemNetworkContextManager(const std::string& user_agent);

  network::mojom::NetworkContextParamsPtr
  CreateSystemNetworkContextManagerParams();

  std::string user_agent_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;

  mojo::Remote<network::mojom::NetworkContext> system_network_context_;

  // Initialized on first access.
  std::unique_ptr<net_log::NetExportFileWriter> net_export_file_writer_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
