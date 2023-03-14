// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include "components/safe_browsing/content/browser/base_ui_manager.h"

#include "components/safe_browsing/content/browser/ui_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class RenderProcessHost;
}  // namespace content

namespace blink {
class URLLoaderThrottle;
}

namespace network {
namespace mojom {
class NetworkContext;
}
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class UrlCheckerDelegate;
class RealTimeUrlLookupServiceBase;
class RemoteSafeBrowsingDatabaseManager;
class SafeBrowsingApiHandlerBridge;
class SafeBrowsingNetworkContext;
class TriggerManager;
}  // namespace safe_browsing

namespace weblayer {
class UrlCheckerDelegateImpl;

// Class for managing safebrowsing related functionality. In particular this
// class owns both the safebrowsing database and UI managers and provides
// support for initialization and construction of these objects.
class SafeBrowsingService {
 public:
  explicit SafeBrowsingService(const std::string& user_agent);

  SafeBrowsingService(const SafeBrowsingService&) = delete;
  SafeBrowsingService& operator=(const SafeBrowsingService&) = delete;

  ~SafeBrowsingService();

  // Executed on UI thread
  void Initialize();
  std::unique_ptr<blink::URLLoaderThrottle> CreateURLLoaderThrottle(
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      int frame_tree_node_id,
      safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service);
  std::unique_ptr<content::NavigationThrottle>
  MaybeCreateSafeBrowsingNavigationThrottleFor(
      content::NavigationHandle* handle);
  void AddInterface(service_manager::BinderRegistry* registry,
                    content::RenderProcessHost* render_process_host);
  void StopDBManager();

  network::mojom::NetworkContext* GetNetworkContext();

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // May be called on the UI or IO thread. The instance returned should be
  // *accessed* only on the IO thread.
  scoped_refptr<safe_browsing::RemoteSafeBrowsingDatabaseManager>
  GetSafeBrowsingDBManager();

  scoped_refptr<safe_browsing::SafeBrowsingUIManager>
  GetSafeBrowsingUIManager();

  safe_browsing::TriggerManager* GetTriggerManager();

 private:
  // Executed on IO thread
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
  GetSafeBrowsingUrlCheckerDelegate();

  // Safe to call multiple times; invocations after the first will be no-ops.
  void StartSafeBrowsingDBManagerOnSBThread();
  void CreateSafeBrowsingUIManager();
  void CreateTriggerManager();
  void CreateAndStartSafeBrowsingDBManager();
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryOnIOThread();
  void CreateURLLoaderFactoryForIO(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);
  void StopDBManagerOnSBThread();

  // The UI manager handles showing interstitials. Accessed on both UI and IO
  // thread.
  scoped_refptr<safe_browsing::SafeBrowsingUIManager> ui_manager_;

  // This is what owns the URLRequestContext inside the network service. This
  // is used by SimpleURLLoader for Safe Browsing requests.
  std::unique_ptr<safe_browsing::SafeBrowsingNetworkContext> network_context_;

  // May be created on UI thread and have references obtained to it on that
  // thread for later passing to the IO thread, but should be *accessed* only
  // on the IO thread.
  scoped_refptr<safe_browsing::RemoteSafeBrowsingDatabaseManager>
      safe_browsing_db_manager_;

  // A SharedURLLoaderFactory and its remote used on the IO thread.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_on_io_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_on_io_;

  scoped_refptr<UrlCheckerDelegateImpl> safe_browsing_url_checker_delegate_;

  std::unique_ptr<safe_browsing::SafeBrowsingApiHandlerBridge>
      safe_browsing_api_handler_;

  std::string user_agent_;

  // Whether |safe_browsing_db_manager_| has been started. Accessed only on the
  // IO thread.
  bool started_db_manager_ = false;

  // Collects data and sends reports to Safe Browsing. Accessed on UI thread.
  std::unique_ptr<safe_browsing::TriggerManager> trigger_manager_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
