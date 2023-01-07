// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_H_
#define WEBLAYER_PUBLIC_NAVIGATION_H_

#include <string>
#include <vector>

class GURL;

namespace net {
class HttpResponseHeaders;
}

namespace weblayer {
class Page;

// These types are sent over IPC and across different versions. Never remove
// or change the order.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplNavigationState
enum class NavigationState {
  // Waiting to receive initial response data.
  kWaitingResponse = 0,
  // Processing the response.
  kReceivingBytes = 1,
  // The navigation succeeded. Any NavigationObservers would have had
  // NavigationCompleted() called.
  kComplete = 2,
  // The navigation failed. This could be because of an error (in which case
  // IsErrorPage() will return true) or the navigation got turned into a
  // download (in which case IsDownload() will return true).
  // NavigationObservers would have had NavigationFailed() called.
  kFailed = 3,
};

class Navigation {
 public:
  virtual ~Navigation() {}

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  virtual GURL GetURL() = 0;

  // Returns the redirects that occurred on the way to the current page. The
  // current page is the last one in the list (so even when there's no redirect,
  // there will be one entry in the list).
  virtual const std::vector<GURL>& GetRedirectChain() = 0;

  virtual NavigationState GetState() = 0;

  // Returns the status code of the navigation. Returns 0 if the navigation
  // hasn't completed yet or if a response wasn't received.
  virtual int GetHttpStatusCode() = 0;

  // Returns the HTTP response headers. Returns nullptr if the navigation
  // hasn't completed yet or if a response wasn't received.
  virtual const net::HttpResponseHeaders* GetResponseHeaders() = 0;

  // Whether the navigation happened without changing document. Examples of
  // same document navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  // * same page history navigation
  virtual bool IsSameDocument() = 0;

  // Whether the navigation resulted in an error page (e.g. interstitial). Note
  // that if an error page reloads, this will return true even though
  // GetNetErrorCode will be kNoError.
  virtual bool IsErrorPage() = 0;

  // Returns true if this navigation resulted in a download. Returns false if
  // this navigation did not result in a download, or if download status is not
  // yet known for this navigation.  Download status is determined for a
  // navigation when processing final (post redirect) HTTP response headers.
  // This means the only time the embedder can know if it's a download is in
  // NavigationObserver::NavigationFailed.
  virtual bool IsDownload() = 0;

  // Whether the target URL can be handled by the browser's internal protocol
  // handlers, i.e., has a scheme that the browser knows how to process
  // internally. Examples of such URLs are http(s) URLs, data URLs, and file
  // URLs. A typical example of a URL for which there is no internal protocol
  // handler (and for which this method would return false) is an intent:// URL.
  // Added in 89.
  virtual bool IsKnownProtocol() = 0;

  // Returns true if the navigation was stopped before it could complete because
  // NavigationController::Stop() was called.
  virtual bool WasStopCalled() = 0;

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplLoadError
  enum LoadError {
    kNoError = 0,            // Navigation completed successfully.
    kHttpClientError = 1,    // Server responded with 4xx status code.
    kHttpServerError = 2,    // Server responded with 5xx status code.
    kSSLError = 3,           // Certificate error.
    kConnectivityError = 4,  // Problem connecting to server.
    kOtherError = 5,         // An error not listed above or below occurred.
    kSafeBrowsingError = 6,  // Safe browsing error.
  };

  // Return information about the error, if any, that was encountered while
  // loading the page.
  virtual LoadError GetLoadError() = 0;

  // Set a request's header. If the header is already present, its value is
  // overwritten. This function can only be called at two times, during start
  // and redirect. When called during start, the header applies to both the
  // start and redirect. |name| must be rfc 2616 compliant and |value| must
  // not contain '\0', '\n' or '\r'.
  //
  // This function may be used to set the referer. If the referer is set in
  // navigation start, it is reset during the redirect. In other words, if you
  // need to set a referer that applies to redirects, then this must be called
  // from NavigationRedirected().
  virtual void SetRequestHeader(const std::string& name,
                                const std::string& value) = 0;

  // Sets the user-agent string used for this navigation. The user-agent is
  // not sticky, it applies to this navigation only (and any redirects). This
  // function may only be called from NavigationObserver::NavigationStarted().
  // Any value specified during start carries through to a redirect. |value|
  // must not contain any illegal characters as documented in
  // SetRequestHeader().  Setting this to a non empty string will cause the
  // User-Agent Client Hint header values and the values returned by
  // `navigator.userAgentData` to be empty for requests this override is applied
  // to.
  virtual void SetUserAgentString(const std::string& value) = 0;

  // Disables auto-reload for this navigation if the network is down and comes
  // back later. Auto-reload is enabled by default. This function may only be
  // called from NavigationObserver::NavigationStarted().
  virtual void DisableNetworkErrorAutoReload() = 0;

  // Whether the navigation was initiated by the page. Examples of
  // page-initiated navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState
  //  * window.history.forward() or window.history.back()
  //
  // This method returns false for navigations initiated by the WebLayer API.
  virtual bool IsPageInitiated() = 0;

  // Whether the navigation is a reload. Examples of reloads include:
  // * embedder-specified through NavigationController::Reload
  // * page-initiated reloads, e.g. location.reload()
  // * reloads when the network interface is reconnected
  virtual bool IsReload() = 0;

  // Whether the navigation is restoring a page from back-forward cache (see
  // https://web.dev/bfcache/). Since a previously loaded page is being reused,
  // there are some things embedders have to keep in mind such as:
  //   * there will be no NavigationObserver::OnFirstContentfulPaint callbacks
  //   * if an embedder injects code using Tab::ExecuteScript there is no need
  //     to reinject scripts
  virtual bool IsServedFromBackForwardCache() = 0;

  // Returns true if this navigation was initiated by a form submission.
  virtual bool IsFormSubmission() = 0;

  // Returns the referrer for this request.
  virtual GURL GetReferrer() = 0;

  // Returns the Page object this navigation is occurring for. This method may
  // only be called in or after NavigationObserver::NavigationCompleted() or
  // NavigationObserve::NavigationFailed(). It can return null if the navigation
  // didn't commit (e.g. 204/205 or download).
  virtual Page* GetPage() = 0;

  // Returns the offset between the indices of the previous last committed and
  // the newly committed navigation entries (e.g. -1 for back navigations, 0
  // for reloads, 1 for forward navigations). This may not cover all corner
  // cases, and can be incorrect in cases like main frame client redirects.
  virtual int GetNavigationEntryOffset() = 0;

  // Returns true if the navigation response was fetched from the cache.
  virtual bool WasFetchedFromCache() = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_H_
