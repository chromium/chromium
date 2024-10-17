// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/autocomplete/wolvic_password_manager_client.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/renderer_forms_with_server_predictions.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/android/window_android.h"
#include "wolvic/browser/autocomplete/wolvic_password_form_util.h"
#include "wolvic/jni_headers/PasswordManager_jni.h"
#include "wolvic/wolvic_browser_context.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace wolvic {

// static
void WolvicPasswordManagerClient::CreateForWebContents(
    content::WebContents* contents) {
  if (FromWebContents(contents)) {
    return;
  }

  contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new WolvicPasswordManagerClient(contents)));
}

WolvicPasswordManagerClient::WolvicPasswordManagerClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WolvicPasswordManagerClient>(*web_contents),
      password_manager_(this),
      password_feature_manager_(GetPrefs(), GetLocalStatePrefs(), nullptr),
      httpauth_manager_(this),
      credentials_filter_(this),
      helper_(this),
      log_manager_(autofill::LogManager::CreateBuffering()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  password_manager::ContentPasswordManagerDriverFactory::CreateForWebContents(
      web_contents, this);

  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);

  JNIEnv* env = AttachCurrentThread();
  java_obj_ = Java_PasswordManager_create(
      env, reinterpret_cast<intptr_t>(this));
}

WolvicPasswordManagerClient::~WolvicPasswordManagerClient() = default;

void WolvicPasswordManagerClient::OnLoginSaved(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  if (!save_password_callback_)
    return;

  auto form =
      GetPasswordFormFromJavaObject(env, ScopedJavaLocalRef<jobject>(jobj));
  std::move(save_password_callback_).Run(form);
}

void WolvicPasswordManagerClient::OnLoginSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  if (!credentials_callback_)
    return;
  auto form =
      GetPasswordFormFromJavaObject(env, ScopedJavaLocalRef<jobject>(jobj));
  std::move(credentials_callback_).Run(&form);
}

void WolvicPasswordManagerClient::OnDismissed(JNIEnv* env) {
  save_password_callback_.Reset();
  credentials_callback_.Reset();
}

void WolvicPasswordManagerClient::HandleSavePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
  password_manager::PasswordForm& saved_form) {
  // Avoid DCHECK when adding the new ID/PW via PasswordFormManagerForUI::Update
  saved_form.federation_origin = url::Origin::Create(GURL(saved_form.url));
  form_to_save->Update(saved_form);
  form_to_save->Save();
}

password_manager::ContentPasswordManagerDriverFactory*
WolvicPasswordManagerClient::GetDriverFactory() const {
  return password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
      web_contents());
}

bool WolvicPasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  ui::WindowAndroid* window_android =
      web_contents()->GetTopLevelNativeWindow();
  if (!window_android)
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_PasswordManager_isSavingAndFillingEnabled(
      env, java_obj_, window_android->GetJavaObject());
}

bool WolvicPasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  ui::WindowAndroid* window_android =
      web_contents()->GetTopLevelNativeWindow();
  if (!window_android)
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_PasswordManager_isFillingEnabled(
      env, java_obj_, window_android->GetJavaObject());
}

bool WolvicPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool is_update) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  const auto& form = form_to_save->GetPendingCredentials();
  if (form.IsSingleUsername())
    return false;

  ScopedJavaLocalRef<jobject> j_form = CreatePasswordFormJavaObject(env, form);

  for (const auto& match_form : form_to_save->GetBestMatches()) {
    if (match_form.username_value == form.username_value) {
      SetGuidToPasswordFormJavaObject(env, j_form, match_form.keychain_identifier);
    }
  }

  save_password_callback_ =
      base::BindOnce(&WolvicPasswordManagerClient::HandleSavePassword,
                     base::Unretained(this), std::move(form_to_save));

  return Java_PasswordManager_saveOrUpdatePassword(env, java_obj_, j_form);
}

bool WolvicPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin, CredentialsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_forms.size())
    return false;

  credentials_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> java_credentials_array =
      CreatePasswordFormJavaArray(env, static_cast<int>(local_forms.size()));

  int index = 0;
  for (const auto& password_form : local_forms) {
    ScopedJavaLocalRef<jobject> java_credential =
        CreatePasswordFormJavaObject(env, *password_form);
    env->SetObjectArrayElement(java_credentials_array.obj(), index,
                               java_credential.obj());
    index++;
  }

  return Java_PasswordManager_chooseCredentials(
      env, java_obj_, java_credentials_array);
}

void WolvicPasswordManagerClient::ShowPasswordManagerErrorMessage(
    password_manager::ErrorMessageFlowType flow_type,
    password_manager::PasswordStoreBackendErrorType error_type) {
  std::string error_type_string;
  switch (error_type) {
    case password_manager::PasswordStoreBackendErrorType::kUncategorized:
      error_type_string = "Uncategorized";
      break;
    case password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable:
      error_type_string = "AuthErrorResolvable";
      break;
    case password_manager::PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
      error_type_string = "AuthErrorUnresolvable";
      break;
    default:
      break;
  }

  LOG(ERROR) << "[Wolvic] Password manager has the " << error_type_string
             << " error during "
             << (flow_type == password_manager::ErrorMessageFlowType::kSaveFlow ?
                  "save flow " : "filling flow");
}

void WolvicPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin) {
  LOG(ERROR) << "Password manager should not run auto signin";
}

void WolvicPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<password_manager::PasswordForm> form) {
  LOG(ERROR) << "Password manager will ignore auto signin";
}

void WolvicPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI>
        submitted_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordManager_notifySuccessfulLoginWithExistingPassword(
      env, java_obj_,
      CreatePasswordFormJavaObject(
          env, submitted_manager->GetPendingCredentials()));
}

void WolvicPasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
}

void WolvicPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<password_manager::PasswordFormManagerForUI>
        saved_form_manager,
    bool is_update_confirmation) {
  // Not implemented to generate password
}

void WolvicPasswordManagerClient::PasswordWasAutofilled(
    base::span<const password_manager::PasswordForm> best_matches,
    const url::Origin& origin,
    const std::vector<raw_ptr<const password_manager::PasswordForm,
                              VectorExperimental>>* federated_matches,
    bool was_autofilled_on_pageload) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!best_matches.size() || !best_matches[0].primary_key.has_value())
    return;

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordManager_onPasswordAutofilled(
      env, java_obj_, CreatePasswordFormJavaObject(env, best_matches[0]));
}

void WolvicPasswordManagerClient::AutofillHttpAuth(
    const password_manager::PasswordForm& preferred_match,
    const password_manager::PasswordFormManagerForUI* form_manager) {
  httpauth_manager_.Autofill(preferred_match, form_manager);
  DCHECK(!form_manager->GetBestMatches().empty());
  PasswordWasAutofilled(form_manager->GetBestMatches(),
                        url::Origin::Create(form_manager->GetURL()), nullptr,
                        /*was_autofilled_on_pageload=*/false);
}

void WolvicPasswordManagerClient::NotifyKeychainError() {
  NOTIMPLEMENTED();
}

const password_manager::PasswordManager*
WolvicPasswordManagerClient::GetPasswordManager() const {
  return &password_manager_;  
}

const password_manager::PasswordFeatureManager*
WolvicPasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

password_manager::HttpAuthManager*
WolvicPasswordManagerClient::GetHttpAuthManager() {
  return &httpauth_manager_;
}

ukm::SourceId WolvicPasswordManagerClient::GetUkmSourceId() {
  return web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

scoped_refptr<network::SharedURLLoaderFactory>
WolvicPasswordManagerClient::GetURLLoaderFactory() {
  return web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

network::mojom::NetworkContext*
WolvicPasswordManagerClient::GetNetworkContext() const {
  return web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext();
}

PrefService* WolvicPasswordManagerClient::GetPrefs() const {
  return WolvicBrowserContext::FromWebContents(*web_contents())
      ->GetPrefService();
}

PrefService* WolvicPasswordManagerClient::GetLocalStatePrefs() const {
  return WolvicBrowserContext::FromWebContents(*web_contents())
      ->GetPrefService();
}

password_manager::PasswordStoreInterface*
WolvicPasswordManagerClient::GetProfilePasswordStore() const {
  return WolvicBrowserContext::FromWebContents(*web_contents())
      ->GetPasswordStore();
}

password_manager::FieldInfoManager*
WolvicPasswordManagerClient::GetFieldInfoManager() const {
  return WolvicBrowserContext::FromWebContents(*web_contents())
      ->GetFieldInfoManager();
}

// Not implemented methods
const syncer::SyncService*
WolvicPasswordManagerClient::GetSyncService() const {
  return nullptr;
}

affiliations::AffiliationService*
WolvicPasswordManagerClient::GetAffiliationService() {
  return nullptr;
}

password_manager::PasswordStoreInterface*
WolvicPasswordManagerClient::GetAccountPasswordStore() const {
  return nullptr;
}

password_manager::PasswordReuseManager*
WolvicPasswordManagerClient::GetPasswordReuseManager() const {
  return nullptr;
}

const password_manager::CredentialsFilter*
WolvicPasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

autofill::LogManager* WolvicPasswordManagerClient::GetLogManager() {
  return log_manager_.get();
}

safe_browsing::PasswordProtectionService*
WolvicPasswordManagerClient::GetPasswordProtectionService() const {
  return nullptr;
}

#if defined(ON_FOCUS_PING_ENABLED)
void WolvicPasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action, const GURL& frame_url) {}
#endif

password_manager::PasswordManagerMetricsRecorder*
WolvicPasswordManagerClient::GetMetricsRecorder() { return nullptr; }

signin::IdentityManager*
WolvicPasswordManagerClient::GetIdentityManager() {
  return WolvicBrowserContext::FromWebContents(*web_contents())
      ->GetIdentityManager();
}

password_manager::WebAuthnCredentialsDelegate*
WolvicPasswordManagerClient::GetWebAuthnCredentialsDelegateForDriver(
      password_manager::PasswordManagerDriver* driver) { return nullptr; }

bool WolvicPasswordManagerClient::IsNewTabPage() const { return false; }

autofill::LanguageCode WolvicPasswordManagerClient::GetPageLanguage() const {
  return autofill::LanguageCode();
}

bool WolvicPasswordManagerClient::WasLastNavigationHTTPError() const {
  DCHECK(web_contents());

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry)
    return false;
  int http_status_code = entry->GetHttpStatusCode();
  return http_status_code >= 400 && http_status_code < 600;
}

net::CertStatus WolvicPasswordManagerClient::GetMainFrameCertStatus() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  return entry ? entry->GetSSL().cert_status : 0;
}

bool WolvicPasswordManagerClient::IsOffTheRecord() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

bool WolvicPasswordManagerClient::IsCommittedMainFrameSecure() const {
  return network::IsOriginPotentiallyTrustworthy(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

const GURL& WolvicPasswordManagerClient::GetLastCommittedURL() const {
  return web_contents()->GetLastCommittedURL();
}

url::Origin WolvicPasswordManagerClient::GetLastCommittedOrigin() const {
  DCHECK(web_contents());
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

bool WolvicPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  return site_isolation::SiteIsolationPolicy::
      IsIsolationForPasswordSitesEnabled();
}

void WolvicPasswordManagerClient::OnFieldTypesDetermined(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    FieldTypeSource source) {
  if (source == FieldTypeSource::kHeuristicsOrAutocomplete) {
    return;
  }

  absl::optional<autofill::RendererFormsWithServerPredictions>
      forms_and_predictions =
          autofill::RendererFormsWithServerPredictions::FromBrowserForm(
              manager, form_id);
  if (!forms_and_predictions) {
    return;
  }

  for (const auto& [form, rfh_id] : forms_and_predictions->renderer_forms) {
    auto* rfh = content::RenderFrameHost::FromID(rfh_id);
    if (!rfh) {
      continue;
    }
    auto* driver =
        password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
            rfh);
    if (!driver) {
      continue;
    }
    password_manager_.ProcessAutofillPredictions(
        driver, form, forms_and_predictions->predictions);
  }
}

void WolvicPasswordManagerClient::PrimaryPageChanged(content::Page& page) {
  httpauth_manager_.OnDidFinishMainFrameNavigation();
}

void WolvicPasswordManagerClient::WebContentsDestroyed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  save_password_callback_.Reset();
  credentials_callback_.Reset();

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordManager_dismissPrompt(env, java_obj_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WolvicPasswordManagerClient);

}  // namespace wolvic
