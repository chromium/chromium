// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_SIGNIN_CLIENT_H_
#define WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_SIGNIN_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/public/base/signin_client.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace wolvic {
class WolvicBrowserContext;
}

namespace wolvic {

class WolvicSigninClient : public SigninClient {
 public:
  explicit WolvicSigninClient(WolvicBrowserContext* context);

  WolvicSigninClient(const WolvicSigninClient&) = delete;
  WolvicSigninClient& operator=(const WolvicSigninClient&) = delete;

  ~WolvicSigninClient() override;

  void DoFinalInit() override;

  // SigninClient implementation.
  PrefService* GetPrefs() override;
  bool IsClearPrimaryAccountAllowed(bool has_sync_account) const override;
  bool IsRevokeSyncConsentAllowed() const override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric,
      bool has_sync_account) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  bool AreSigninCookiesAllowed() override;
  bool AreSigninCookiesDeletedOnExit() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  bool AreNetworkCallsDelayed() override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;
  version_info::Channel GetClientChannel() override;
  void OnPrimaryAccountChanged(
      signin::PrimaryAccountChangeEvent event_details) override;

 private:
  // Returns what kind of signout is possible given `has_sync_account` and the
  // optional `signout_source`. If `signout_source` is provided, it will be
  // check against some sources that must always allow signout regardless of any
  // restriction, otherwise the decision is made based on the profile's status.
  SigninClient::SignoutDecision GetSignoutDecision(
      bool has_sync_account,
      const std::optional<signin_metrics::ProfileSignout> signout_source)
      const;
  void VerifySyncToken();
  void OnCloseBrowsersSuccess(
      const signin_metrics::ProfileSignout signout_source_metric,
      bool has_sync_account,
      const base::FilePath& profile_path);
  void OnCloseBrowsersAborted(const base::FilePath& profile_path);

  raw_ptr<WolvicBrowserContext> context_;

  // Stored callback from PreSignOut();
  base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached_;
};

} // namespace wolvic

#endif  // WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_SIGNIN_CLIENT_H_
