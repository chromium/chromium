// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/signin_url_loader_throttle.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/google_accounts_delegate.h"

namespace weblayer {

const char kSignOutPath[] = "/SignOutOptions";

namespace {
constexpr char kWebLayerMirrorHeaderSource[] = "WebLayer";

GoogleAccountsDelegate* GetDelegate(
    const content::WebContents::Getter& web_contents_getter) {
  auto* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return nullptr;

  auto* tab = TabImpl::FromWebContents(web_contents);
  if (!tab)
    return nullptr;

  return tab->google_accounts_delegate();
}

void ProcessMirrorHeader(content::WebContents::Getter web_contents_getter,
                         const signin::ManageAccountsParams& params) {
  auto* delegate = GetDelegate(web_contents_getter);
  if (delegate)
    delegate->OnGoogleAccountsRequest(params);
}

void MaybeAddQueryParams(GURL* url) {
  // Add manage=true to query parameters for sign out URLs to make sure we
  // receive the Mirror response headers instead of the normal sign out page.
  if (gaia::HasGaiaSchemeHostPort(*url) && url->path_piece() == kSignOutPath) {
    *url = net::AppendOrReplaceQueryParameter(*url, "manage", "true");
  }
}
}  // namespace

SigninURLLoaderThrottle::~SigninURLLoaderThrottle() = default;

// static
std::unique_ptr<SigninURLLoaderThrottle> SigninURLLoaderThrottle::Create(
    content::BrowserContext* browser_context,
    content::WebContents::Getter web_contents_getter) {
  if (!GetDelegate(web_contents_getter))
    return nullptr;

  // Use base::WrapUnique + new because of the constructor is private.
  return base::WrapUnique(new SigninURLLoaderThrottle(
      browser_context, std::move(web_contents_getter)));
}

void SigninURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  GoogleAccountsDelegate* delegate = GetDelegate(web_contents_getter_);
  if (!delegate)
    return;

  MaybeAddQueryParams(&request->url);

  request_url_ = request->url;
  is_main_frame_ =
      static_cast<blink::mojom::ResourceType>(request->resource_type) ==
      blink::mojom::ResourceType::kMainFrame;

  net::HttpRequestHeaders modified_request_headers;
  std::vector<std::string> to_be_removed_request_headers;
  ProcessRequest(GURL(), &request->headers, &to_be_removed_request_headers,
                 &modified_request_headers);
  signin::RequestAdapter adapter(request_url_, request->headers,
                                 &modified_request_headers,
                                 &to_be_removed_request_headers);
  request_headers_.CopyFrom(request->headers);
}

void SigninURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* headers_to_remove,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  if (!GetDelegate(web_contents_getter_))
    return;

  MaybeAddQueryParams(&redirect_info->new_url);

  ProcessRequest(redirect_info->new_url, &request_headers_, headers_to_remove,
                 modified_headers);
  ProcessResponse(response_head.headers.get());

  request_url_ = redirect_info->new_url;
}

void SigninURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (!GetDelegate(web_contents_getter_))
    return;

  ProcessResponse(response_head->headers.get());
}

SigninURLLoaderThrottle::SigninURLLoaderThrottle(
    content::BrowserContext* browser_context,
    content::WebContents::Getter web_contents_getter)
    : browser_context_(browser_context),
      web_contents_getter_(std::move(web_contents_getter)) {}

void SigninURLLoaderThrottle::ProcessRequest(
    const GURL& new_url,
    net::HttpRequestHeaders* original_headers,
    std::vector<std::string>* headers_to_remove,
    net::HttpRequestHeaders* modified_headers) {
  GoogleAccountsDelegate* delegate = GetDelegate(web_contents_getter_);
  if (!delegate)
    return;

  signin::RequestAdapter request_adapter(request_url_, *original_headers,
                                         modified_headers, headers_to_remove);
  // Disable incognito and adding accounts for now. This shouldn't matter in
  // practice though since we are skipping the /SignOutOptions page completely
  // with the manage=true param.
  //
  // TODO(crbug.com/1134042): Check whether the child account status should also
  // be sent in the Mirror request header from WebLayer.
  signin::AppendOrRemoveMirrorRequestHeader(
      &request_adapter, new_url, delegate->GetGaiaId(),
      /*is_child_account=*/signin::Tribool::kUnknown,
      signin::AccountConsistencyMethod::kMirror,
      CookieSettingsFactory::GetForBrowserContext(browser_context_).get(),
      signin::PROFILE_MODE_INCOGNITO_DISABLED |
          signin::PROFILE_MODE_ADD_ACCOUNT_DISABLED,
      kWebLayerMirrorHeaderSource, /*force_account_consistency=*/true);

  original_headers->MergeFrom(*modified_headers);
  for (const std::string& name : *headers_to_remove)
    original_headers->RemoveHeader(name);
}

void SigninURLLoaderThrottle::ProcessResponse(
    const net::HttpResponseHeaders* headers) {
  if (!gaia::HasGaiaSchemeHostPort(request_url_) || !is_main_frame_ ||
      !headers) {
    return;
  }

  std::string header_value;
  if (!headers->GetNormalizedHeader(signin::kChromeManageAccountsHeader,
                                    &header_value)) {
    return;
  }

  signin::ManageAccountsParams params =
      signin::BuildManageAccountsParams(header_value);
  if (params.service_type == signin::GAIA_SERVICE_TYPE_NONE)
    return;

  // Only process one mirror header per request (multiple headers on the same
  // redirect chain are ignored).
  if (response_header_processed_) {
    LOG(ERROR) << "Multiple X-Chrome-Manage-Accounts headers on a redirect "
               << "chain, ignoring";
    return;
  }

  response_header_processed_ = true;

  // Post a task even if we are already on the UI thread to avoid making any
  // requests while processing a throttle event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ProcessMirrorHeader, web_contents_getter_, params));
}

}  // namespace weblayer
