// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/youtube/youtube_url_loader_request_interceptor.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_ui_data.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Helper to configure an artificial redirect to `new_url`. This configures
// `response_head` and returns a computed RedirectInfo so both can be passed to
// URLLoaderClient::OnReceiveRedirect() to trigger the redirect.
net::RedirectInfo SetupRedirect(
    const network::ResourceRequest& request,
    const GURL& new_url,
    network::mojom::URLResponseHead* response_head) {
  response_head->encoded_data_length = 0;
  response_head->request_start = base::TimeTicks::Now();
  response_head->response_start = response_head->request_start;
  std::string header_string = base::StringPrintf(
      "HTTP/1.1 %i Temporary Redirect\n"
      "Location: %s\n",
      net::HTTP_TEMPORARY_REDIRECT, new_url.spec().c_str());
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_string));
  net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
      request.method, request.url, request.site_for_cookies,
      request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      request.referrer_policy, request.referrer.spec(),
      net::HTTP_TEMPORARY_REDIRECT, new_url,
      /*referrer_policy_header=*/absl::nullopt,
      /*insecure_scheme_was_upgraded=*/false);
  return redirect_info;
}

// Checks that the given url matches youtube.com or youtube-nocookie.com.
bool URLMatchesYoutube(const GURL& url) {
  url_matcher::URLMatcherConditionFactory factory;
  auto host_equals_youtube = factory.CreateHostContainsCondition("youtube.com");
  auto host_equals_youtube_nocookie =
      factory.CreateHostContainsCondition("youtube-nocookie.com");
  return host_equals_youtube.IsMatch(
             {host_equals_youtube.string_pattern()->id()}, url) ||
         host_equals_youtube_nocookie.IsMatch(
             {host_equals_youtube_nocookie.string_pattern()->id()}, url);
}

// Ensures that "app=desktop" query parameter is present in the url.
GURL EnsureAppDesktopParamIsPresent(const GURL& url) {
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(url.query(), '=', '&', &pairs);

  bool found_app = false;
  for (auto& pair : pairs) {
    if (pair.first == "app") {
      pair.second = "desktop";
      found_app = true;
    }
  }

  if (!found_app) {
    pairs.push_back(std::make_pair("app", "desktop"));
  }

  std::vector<std::string> query_params;
  for (auto& pair : pairs) {
    query_params.push_back(pair.first + "=" + pair.second);
  }

  GURL::Replacements replacements;
  std::string query = base::JoinString(query_params, "&");
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

}  // namespace

namespace wolvic {

YoutubeURLLoaderRequestInterceptor::YoutubeURLLoaderRequestInterceptor() =
    default;

YoutubeURLLoaderRequestInterceptor::~YoutubeURLLoaderRequestInterceptor() =
    default;

void YoutubeURLLoaderRequestInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only intercept requests for youtube.com.
  if (!URLMatchesYoutube(tentative_resource_request.url)) {
    std::move(callback).Run({});
    return;
  }

  // Add a parameter "app=desktop" to the URL if needed and redirect to it.
  GURL new_url = EnsureAppDesktopParamIsPresent(tentative_resource_request.url);
  if (new_url == tentative_resource_request.url) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(CreateRedirectHandler(new_url));
}

YoutubeURLLoaderRequestInterceptor::RequestHandler
YoutubeURLLoaderRequestInterceptor::CreateRedirectHandler(const GURL& new_url) {
  return base::BindOnce(&YoutubeURLLoaderRequestInterceptor::RedirectHandler,
                        weak_factory_.GetWeakPtr(), new_url);
}

void YoutubeURLLoaderRequestInterceptor::RedirectHandler(
    const GURL& new_url,
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set up Mojo connection and initiate the redirect. `client_` and `receiver_`
  // may have been previously bound from handling a previous upgrade earlier in
  // the same navigation, so reset them before re-binding them to handle a new
  // redirect.
  receiver_.reset();
  client_.reset();
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&YoutubeURLLoaderRequestInterceptor::OnConnectionClosed,
                     weak_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));

  // Create redirect.
  auto response_head = network::mojom::URLResponseHead::New();
  net::RedirectInfo redirect_info =
      SetupRedirect(request, new_url, response_head.get());

  client_->OnReceiveRedirect(redirect_info, std::move(response_head));
}

void YoutubeURLLoaderRequestInterceptor::OnConnectionClosed() {
  // This happens when content cancels the navigation. Reset the receiver and
  // client handle.
  receiver_.reset();
  client_.reset();
}

}  // namespace wolvic
