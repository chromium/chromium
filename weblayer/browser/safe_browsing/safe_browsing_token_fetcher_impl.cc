// Copyright 2021 The Chromium Authors. All rights reserved.
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

  // NOTE: base::Unretained() is safe below as this object owns
  // |token_fetch_tracker_|, and the callback will not be invoked after
  // |token_fetch_tracker_| is destroyed.
  const int request_id = token_fetch_tracker_.StartTrackingTokenFetch(
      std::move(callback),
      base::BindOnce(&SafeBrowsingTokenFetcherImpl::OnTokenTimeout,
                     base::Unretained(this)));
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
}

void SafeBrowsingTokenFetcherImpl::OnTokenTimeout(int request_id) {
  request_ids_.erase(request_id);
}

}  // namespace weblayer
