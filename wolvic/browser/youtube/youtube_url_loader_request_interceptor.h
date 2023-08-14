// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_BROWSER_YOUTUBE_YOUTUBE_URL_LOADER_REQUEST_INTERCEPTOR_H_
#define WOLVIC_WOLVIC_BROWSER_YOUTUBE_YOUTUBE_URL_LOADER_REQUEST_INTERCEPTOR_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace wolvic {

// Intercepts requests to youtube.com and makes sure that the desktop version is
// used by adding an "app=desktop" query parameter.
// Parts of this implementation are taken from
// chrome/browser/ssl/https_upgrades_interceptor.h.
class YoutubeURLLoaderRequestInterceptor
    : public content::URLLoaderRequestInterceptor,
      public network::mojom::URLLoader {
 public:
  YoutubeURLLoaderRequestInterceptor();

  YoutubeURLLoaderRequestInterceptor(
      const YoutubeURLLoaderRequestInterceptor&) = delete;
  YoutubeURLLoaderRequestInterceptor& operator=(
      const YoutubeURLLoaderRequestInterceptor&) = delete;

  ~YoutubeURLLoaderRequestInterceptor() override;

  // content::URLLoaderRequestInterceptor overrides.
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      LoaderCallback callback) override;

 private:
  // network::mojom::URLLoader overrides.
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  // Returns a RequestHandler callback that can be passed to the underlying
  // LoaderCallback to serve an artificial redirect to `new_url`.
  RequestHandler CreateRedirectHandler(const GURL& new_url);

  // Passed to the LoaderCallback as the ResponseHandler with `new_url` bound,
  // this method receives the receiver and client_remote from the
  // NavigationLoader, to bind against. Triggers a redirect to `new_url` using
  // `client`.
  void RedirectHandler(
      const GURL& new_url,
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Mojo error handling. Resets `receiver_` and `client_`.
  void OnConnectionClosed();

  // Receiver for the URLLoader interface.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};

  // The owning client. Used for serving redirects.
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<YoutubeURLLoaderRequestInterceptor> weak_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_BROWSER_YOUTUBE_YOUTUBE_URL_LOADER_REQUEST_INTERCEPTOR_H_
