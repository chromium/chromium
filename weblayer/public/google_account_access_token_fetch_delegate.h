// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_GOOGLE_ACCOUNT_ACCESS_TOKEN_FETCH_DELEGATE_H_
#define WEBLAYER_PUBLIC_GOOGLE_ACCOUNT_ACCESS_TOKEN_FETCH_DELEGATE_H_

#include <set>
#include <string>

#include "base/callback_forward.h"

namespace weblayer {

using OnTokenFetchedCallback =
    base::OnceCallback<void(const std::string& token)>;

// An interface that allows clients to handle access token requests for Google
// accounts originating in the browser.
class GoogleAccountAccessTokenFetchDelegate {
 public:
  // Called when the WebLayer implementation wants to fetch an access token for
  // the embedder's current GAIA account (if any) and the given scopes. The
  // client should invoke |onTokenFetchedCallback| when its internal token fetch
  // is complete, passing either the fetched access token or the empty string in
  // the case of failure (e.g., if there is no current GAIA account or there was
  // an error in the token fetch).
  //
  // NOTE: WebLayer will not perform any caching of the returned token but will
  // instead make a new request each time that it needs to use an access token.
  // The expectation is that the client will use caching internally to minimize
  // latency of these requests.
  virtual void FetchAccessToken(const std::set<std::string>& scopes,
                                OnTokenFetchedCallback callback) = 0;

  // Called when a token previously obtained via a call to
  // FetchAccessToken(|scopes|) is identified as invalid, so the embedder can
  // take appropriate action (e.g., dropping the token from its cache and/or
  // force-fetching a new token).
  virtual void OnAccessTokenIdentifiedAsInvalid(
      const std::set<std::string>& scopes,
      const std::string& token) = 0;

 protected:
  virtual ~GoogleAccountAccessTokenFetchDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_GOOGLE_ACCOUNT_ACCESS_TOKEN_FETCH_DELEGATE_H_
