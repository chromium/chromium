// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_token_fetcher_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_constants.h"
#include "weblayer/public/google_account_access_token_fetch_delegate.h"

namespace weblayer {

SafeBrowsingTokenFetcherImpl::SafeBrowsingTokenFetcherImpl(
    const AccessTokenFetchDelegateGetter& delegate_getter)
    : delegate_getter_(delegate_getter) {}

SafeBrowsingTokenFetcherImpl::~SafeBrowsingTokenFetcherImpl() = default;

void SafeBrowsingTokenFetcherImpl::Start(Callback callback) {
  auto* delegate = delegate_getter_.Run();

  if (!delegate) {
    std::move(callback).Run("");
    return;
  }

  // NOTE: When a token fetch timeout occurs |token_fetch_tracker_| will invoke
  // the client callback, which may end up synchronously destroying this object
  // before this object's own callback is invoked. Hence we bind our own
  // callback via a WeakPtr.
  const int request_id = token_fetch_tracker_.StartTrackingTokenFetch(
      std::move(callback),
      base::BindOnce(&SafeBrowsingTokenFetcherImpl::OnTokenTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
  request_ids_.insert(request_id);

  // In contrast, this object does *not* have a determined lifetime relationship
  // with |delegate|.
  delegate->FetchAccessToken(
      {GaiaConstants::kChromeSafeBrowsingOAuth2Scope},
      base::BindOnce(&SafeBrowsingTokenFetcherImpl::OnTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void SafeBrowsingTokenFetcherImpl::OnInvalidAccessToken(
    const std::string& invalid_access_token) {
  auto* delegate = delegate_getter_.Run();

  if (!delegate)
    return;

  delegate->OnAccessTokenIdentifiedAsInvalid(
      {GaiaConstants::kChromeSafeBrowsingOAuth2Scope}, invalid_access_token);
}

void SafeBrowsingTokenFetcherImpl::OnTokenFetched(
    int request_id,
    const std::string& access_token) {
  if (!request_ids_.count(request_id)) {
    // The request timed out before the delegate responded; nothing to do.
    return;
  }

  request_ids_.erase(request_id);

  token_fetch_tracker_.OnTokenFetchComplete(request_id, access_token);

  // NOTE: Calling SafeBrowsingTokenFetchTracker::OnTokenFetchComplete might
  // have resulted in the synchronous destruction of this object, so there is
  // nothing safe to do here but return.
}

void SafeBrowsingTokenFetcherImpl::OnTokenTimeout(int request_id) {
  DCHECK(request_ids_.count(request_id));
  request_ids_.erase(request_id);
}

}  // namespace weblayer
