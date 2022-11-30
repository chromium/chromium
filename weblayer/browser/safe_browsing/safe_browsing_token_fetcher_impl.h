// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TOKEN_FETCHER_IMPL_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TOKEN_FETCHER_IMPL_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetch_tracker.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"

namespace weblayer {

class GoogleAccountAccessTokenFetchDelegate;

// This class fetches access tokens for Safe Browsing via a
// GoogleAccountAccessTokenFetcherDelegate.
class SafeBrowsingTokenFetcherImpl
    : public safe_browsing::SafeBrowsingTokenFetcher {
 public:
  using AccessTokenFetchDelegateGetter =
      base::RepeatingCallback<GoogleAccountAccessTokenFetchDelegate*()>;

  // Create a SafeBrowsingTokenFetcherImpl that makes access token requests via
  // the object returned by |delegate_getter|. |delegate_getter| may return
  // null, in which case this object will return the empty string for access
  // token requests. This object will not cache the pointer returned by
  // |delegate_getter| but will instead invoke it on every access token request,
  // as that object might change over time.
  // NOTE: In production the getter is
  // ProfileImpl::access_token_fetcher_delegate(); this level of indirection is
  // present to support unittests.
  explicit SafeBrowsingTokenFetcherImpl(
      const AccessTokenFetchDelegateGetter& delegate_getter);
  ~SafeBrowsingTokenFetcherImpl() override;

  // SafeBrowsingTokenFetcher:
  void Start(Callback callback) override;
  void OnInvalidAccessToken(const std::string& invalid_access_token) override;

 private:
  void OnTokenFetched(int request_id, const std::string& access_token);
  void OnTokenTimeout(int request_id);

  AccessTokenFetchDelegateGetter delegate_getter_;

  safe_browsing::SafeBrowsingTokenFetchTracker token_fetch_tracker_;

  // IDs of outstanding client access token requests being tracked by
  // |token_fetch_tracker_|.
  std::set<int> request_ids_;

  base::WeakPtrFactory<SafeBrowsingTokenFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TOKEN_FETCHER_IMPL_H_
