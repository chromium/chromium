// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_MANAGER_CLIENT_H_
#define WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_credential_manager.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/sync_credentials_filter.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace password_manager {
class PasswordManagerMetricsRecorder;
}  // namespace password_manager

class PrefService;

namespace wolvic {

class WolvicPasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<WolvicPasswordManagerClient>,
      public autofill::AutofillManager::Observer {
 public:
  static void CreateForWebContents(content::WebContents* contents);
  WolvicPasswordManagerClient(const WolvicPasswordManagerClient&) = delete;
  WolvicPasswordManagerClient& operator=(const WolvicPasswordManagerClient&) =
      delete;

  WolvicPasswordManagerClient(content::WebContents* contents);
  ~WolvicPasswordManagerClient() override;

  void OnLoginSaved(
        JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);
  void OnLoginSelected(
        JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);
  void OnDismissed(JNIEnv* env);

  // password_manager::PasswordManagerClient implementation.
  bool IsSavingAndFillingEnabled(const GURL& url) const override;
  bool IsFillingEnabled(const GURL& url) const override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool is_update) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin,
      password_manager::PasswordManagerClient::CredentialsCallback callback)
          override;
  void ShowPasswordManagerErrorMessage(
      password_manager::ErrorMessageFlowType flow_type,
      password_manager::PasswordStoreBackendErrorType error_type) override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<password_manager::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          submitted_manager) override;
  void NotifyStorePasswordCalled() override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          saved_form_manager,
      bool is_update_confirmation) override;
  void PasswordWasAutofilled(
      base::span<const password_manager::PasswordForm> best_matches,
      const url::Origin& origin,
      const std::vector<raw_ptr<const password_manager::PasswordForm,
                                VectorExperimental>>* federated_matches,
      bool was_autofilled_on_pageload) override;
  void AutofillHttpAuth(
      const password_manager::PasswordForm& preferred_match,
      const password_manager::PasswordFormManagerForUI* form_manager) override;
  void NotifyKeychainError() override;

  void PromptUserToMovePasswordToAccount(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move)
          override {}
  void ShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool is_update) override {}
  void HideManualFallbackForSaving() override {}
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override {}

  const password_manager::PasswordManager* GetPasswordManager() const override;
  using password_manager::PasswordManagerClient::GetPasswordFeatureManager;
  const password_manager::PasswordFeatureManager* GetPasswordFeatureManager()
      const override;
  password_manager::HttpAuthManager* GetHttpAuthManager() override;
  ukm::SourceId GetUkmSourceId() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::NetworkContext* GetNetworkContext() const override;

  PrefService* GetPrefs() const override;
  PrefService* GetLocalStatePrefs() const override;
  const syncer::SyncService* GetSyncService() const override;
  affiliations::AffiliationService* GetAffiliationService() override;
  password_manager::PasswordStoreInterface*
  GetProfilePasswordStore() const override;
  password_manager::PasswordStoreInterface*
  GetAccountPasswordStore() const override;
  password_manager::PasswordReuseManager*
  GetPasswordReuseManager() const override;
  const password_manager::CredentialsFilter*
  GetStoreResultFilter() const override;
  autofill::LogManager* GetLogManager() override;
  safe_browsing::PasswordProtectionService*
  GetPasswordProtectionService() const override;
#if defined(ON_FOCUS_PING_ENABLED)
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
#endif
  password_manager::PasswordManagerMetricsRecorder*
  GetMetricsRecorder() override;
  signin::IdentityManager* GetIdentityManager() override;
  password_manager::FieldInfoManager* GetFieldInfoManager() const override;
  password_manager::WebAuthnCredentialsDelegate*
  GetWebAuthnCredentialsDelegateForDriver(
      password_manager::PasswordManagerDriver* driver) override;
  bool IsNewTabPage() const override;
  autofill::LanguageCode GetPageLanguage() const override;

  bool WasLastNavigationHTTPError() const override;
  net::CertStatus GetMainFrameCertStatus() const override;
  bool IsOffTheRecord() const override;
  bool IsCommittedMainFrameSecure() const override;
  const GURL& GetLastCommittedURL() const override;
  url::Origin GetLastCommittedOrigin() const override;
  bool IsIsolationForPasswordSitesEnabled() const override;

 private:
  friend class content::WebContentsUserData<WolvicPasswordManagerClient>;

  void HandleSavePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      password_manager::PasswordForm& saved_form);

  // content::WebContentsObserver overrides.
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  // autofill::AutofillManager::Observer:
  void OnFieldTypesDetermined(autofill::AutofillManager& manager,
                              autofill::FormGlobalId form_id,
                              FieldTypeSource source) override;

  password_manager::ContentPasswordManagerDriverFactory* GetDriverFactory()
      const;

  password_manager::PasswordManager password_manager_;
  password_manager::PasswordFeatureManagerImpl password_feature_manager_;
  password_manager::HttpAuthManagerImpl httpauth_manager_;

  const password_manager::SyncCredentialsFilter credentials_filter_;

  // A callback to be invoked when user accept to save the password.
  base::OnceCallback<void(password_manager::PasswordForm& saved_form)>
      save_password_callback_;

  // A callback to be invoked when user selects a credential.
  password_manager::PasswordManagerClient::CredentialsCallback
      credentials_callback_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Observes `AutofillManager`s of the `WebContents` that `this` belongs to.
  autofill::ScopedAutofillManagersObservation autofill_managers_observation_{
      this};

  // Helper for performing logic that is common between
  // ChromePasswordManagerClient and IOSChromePasswordManagerClient.
  password_manager::PasswordManagerClientHelper helper_;

  std::unique_ptr<autofill::LogManager> log_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_MANAGER_CLIENT_H_
