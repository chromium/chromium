// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include "components/safe_browsing/content/base_ui_manager.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class RenderProcessHost;
}

namespace blink {
class URLLoaderThrottle;
}

namespace network {
namespace mojom {
class NetworkContext;
}
class SharedURLLoaderFactory;
}

namespace safe_browsing {
class UrlCheckerDelegate;
class RealTimeUrlLookupServiceBase;
class RemoteSafeBrowsingDatabaseManager;
class SafeBrowsingApiHandler;
class SafeBrowsingNetworkContext;
}  // namespace safe_browsing

namespace weblayer {
class UrlCheckerDelegateImpl;

// Class for managing safebrowsing related functionality. In particular this
// class owns both the safebrowsing database and UI managers and provides
// support for initialization and construction of these objects.
class SafeBrowsingService {
 public:
  explicit SafeBrowsingService(const std::string& user_agent);
  ~SafeBrowsingService();

  // Executed on UI thread
  void Initialize();
  std::unique_ptr<blink::URLLoaderThrottle> CreateURLLoaderThrottle(
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      int frame_tree_node_id,
      safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service);
  std::unique_ptr<content::NavigationThrottle>
  CreateSafeBrowsingNavigationThrottle(content::NavigationHandle* handle);
  void AddInterface(service_manager::BinderRegistry* registry,
                    content::RenderProcessHost* render_process_host);
  void StopDBManager();

  network::mojom::NetworkContext* GetNetworkContext();

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

 private:
  SafeBrowsingUIManager* GetSafeBrowsingUIManager();
  safe_browsing::RemoteSafeBrowsingDatabaseManager* GetSafeBrowsingDBManager();

  // Executed on IO thread
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
  GetSafeBrowsingUrlCheckerDelegate();

  void CreateSafeBrowsingUIManager();
  void CreateAndStartSafeBrowsingDBManager();
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryOnIOThread();
  void CreateURLLoaderFactoryForIO(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);
  void StopDBManagerOnIOThread();

  // The UI manager handles showing interstitials. Accessed on both UI and IO
  // thread.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  // This is what owns the URLRequestContext inside the network service. This
  // is used by SimpleURLLoader for Safe Browsing requests.
  std::unique_ptr<safe_browsing::SafeBrowsingNetworkContext> network_context_;

  // Accessed on IO thread only.
  scoped_refptr<safe_browsing::RemoteSafeBrowsingDatabaseManager>
      safe_browsing_db_manager_;

  // A SharedURLLoaderFactory and its remote used on the IO thread.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_on_io_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_on_io_;

  scoped_refptr<UrlCheckerDelegateImpl> safe_browsing_url_checker_delegate_;

  std::unique_ptr<safe_browsing::SafeBrowsingApiHandler>
      safe_browsing_api_handler_;

  std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingService);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
