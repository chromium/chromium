// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PROXYING_URL_LOADER_FACTORY_IMPL_H_
#define WEBLAYER_BROWSER_PROXYING_URL_LOADER_FACTORY_IMPL_H_

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace embedder_support {
class WebResourceResponse;
}

namespace weblayer {

// Used to service navigations when the WebResourceResponse was specified.
// Otherwise it will forward the request to the original URLLoaderFactory.
class ProxyingURLLoaderFactoryImpl : public network::mojom::URLLoaderFactory {
 public:
  ProxyingURLLoaderFactoryImpl(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      const GURL& url_for_response,
      std::unique_ptr<embedder_support::WebResourceResponse> response,
      int frame_tree_node_id,
      int navigation_entry_unique_id);

  ProxyingURLLoaderFactoryImpl(const ProxyingURLLoaderFactoryImpl&) = delete;
  ProxyingURLLoaderFactoryImpl& operator=(const ProxyingURLLoaderFactoryImpl&) =
      delete;

  static bool HasCachedInputStream(int frame_tree_node_id,
                                   int navigation_entry_unique_id);

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

 private:
  ~ProxyingURLLoaderFactoryImpl() override;
  void OnTargetFactoryError();
  void OnProxyBindingError();

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  GURL url_for_response_;
  std::unique_ptr<embedder_support::WebResourceResponse> response_;
  const int frame_tree_node_id_;
  const int navigation_entry_unique_id_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PROXYING_URL_LOADER_FACTORY_IMPL_H_
