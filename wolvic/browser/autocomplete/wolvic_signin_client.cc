// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/autocomplete/wolvic_signin_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "wolvic/wolvic_browser_context.h"

namespace wolvic {

WolvicSigninClient::WolvicSigninClient(WolvicBrowserContext* context)
    : context_(context) {}

WolvicSigninClient::~WolvicSigninClient() = default;

void WolvicSigninClient::DoFinalInit() {
  VerifySyncToken();
}

PrefService* WolvicSigninClient::GetPrefs() { return context_->GetPrefService(); }

scoped_refptr<network::SharedURLLoaderFactory>
WolvicSigninClient::GetURLLoaderFactory() {
  return context_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

network::mojom::CookieManager* WolvicSigninClient::GetCookieManager() {
  return context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

network::mojom::NetworkContext* WolvicSigninClient::GetNetworkContext() {
  return context_->GetDefaultStoragePartition()->GetNetworkContext();
}

bool WolvicSigninClient::AreSigninCookiesAllowed() {
  return false;
}

bool WolvicSigninClient::AreSigninCookiesDeletedOnExit() {
  return false;
}

void WolvicSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
}

void WolvicSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
}

bool WolvicSigninClient::IsClearPrimaryAccountAllowed(
    bool has_sync_account) const {
  return true;
}

bool WolvicSigninClient::IsRevokeSyncConsentAllowed() const {
  return false;
}

void WolvicSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric,
    bool has_sync_account) {
  DCHECK(on_signout_decision_reached);
  DCHECK(!on_signout_decision_reached_) << "SignOut already in-progress!";
  on_signout_decision_reached_ = std::move(on_signout_decision_reached);

    std::move(on_signout_decision_reached_)
        .Run(GetSignoutDecision(has_sync_account, signout_source_metric));
}

bool WolvicSigninClient::AreNetworkCallsDelayed() {
  return false;
}

void WolvicSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  NOTREACHED();
}

std::unique_ptr<GaiaAuthFetcher> WolvicSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return nullptr;
}

version_info::Channel WolvicSigninClient::GetClientChannel() {
  return version_info::Channel::UNKNOWN;
}

void WolvicSigninClient::OnPrimaryAccountChanged(
    signin::PrimaryAccountChangeEvent event_details) {
  NOTIMPLEMENTED();
}

SigninClient::SignoutDecision WolvicSigninClient::GetSignoutDecision(
    bool has_sync_account,
    const std::optional<signin_metrics::ProfileSignout> signout_source) const {
  return SigninClient::SignoutDecision::ALLOW;
}

void WolvicSigninClient::VerifySyncToken() {}

void WolvicSigninClient::OnCloseBrowsersSuccess(
    const signin_metrics::ProfileSignout signout_source_metric,
    bool has_sync_account,
    const base::FilePath& profile_path) {
  std::move(on_signout_decision_reached_)
      .Run(GetSignoutDecision(has_sync_account, signout_source_metric));
}

void WolvicSigninClient::OnCloseBrowsersAborted(
    const base::FilePath& profile_path) {
  // Disallow sign-out (aborted).
  std::move(on_signout_decision_reached_)
      .Run(SignoutDecision::REVOKE_SYNC_DISALLOWED);
}

} // namespace wolvic