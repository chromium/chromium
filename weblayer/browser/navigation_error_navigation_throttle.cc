// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_error_navigation_throttle.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/error_page_helper.mojom.h"
#include "weblayer/public/error_page.h"
#include "weblayer/public/error_page_delegate.h"

using content::NavigationThrottle;

namespace weblayer {

NavigationErrorNavigationThrottle::NavigationErrorNavigationThrottle(
    content::NavigationHandle* handle)
    : NavigationThrottle(handle) {
  // As this calls to the delegate, and the delegate only knows about main
  // frames, this should only be used for main frames.
  DCHECK(handle->IsInMainFrame());
}

NavigationErrorNavigationThrottle::~NavigationErrorNavigationThrottle() =
    default;

NavigationThrottle::ThrottleCheckResult
NavigationErrorNavigationThrottle::WillFailRequest() {
  // The embedder is not allowed to replace ssl error pages.
  if (navigation_handle()->GetNetErrorCode() == net::Error::OK ||
      net::IsCertificateError(navigation_handle()->GetNetErrorCode())) {
    return NavigationThrottle::PROCEED;
  }

  TabImpl* tab =
      TabImpl::FromWebContents(navigation_handle()->GetWebContents());
  // Instances of this class are only created if there is a Tab associated
  // with the WebContents.
  DCHECK(tab);
  if (!tab->error_page_delegate())
    return NavigationThrottle::PROCEED;

  NavigationImpl* navigation =
      static_cast<NavigationControllerImpl*>(tab->GetNavigationController())
          ->GetNavigationImplFromHandle(navigation_handle());
  // The navigation this was created for should always outlive this.
  DCHECK(navigation);
  auto error_page = tab->error_page_delegate()->GetErrorPageContent(navigation);
  if (!error_page)
    return NavigationThrottle::PROCEED;

  mojo::AssociatedRemote<mojom::ErrorPageHelper> remote_error_page_helper;
  navigation_handle()
      ->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&remote_error_page_helper);
  remote_error_page_helper->DisableErrorPageHelperForNextError();

  return NavigationThrottle::ThrottleCheckResult(
      NavigationThrottle::BLOCK_REQUEST, navigation_handle()->GetNetErrorCode(),
      error_page->html);
}

const char* NavigationErrorNavigationThrottle::GetNameForLogging() {
  return "NavigationErrorNavigationThrottle";
}

}  // namespace weblayer
