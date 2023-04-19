// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/autofill_client_impl.h"

#include <utility>

#include "build/build_config.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "weblayer/browser/translate_client_impl.h"

namespace weblayer {

AutofillClientImpl::~AutofillClientImpl() = default;

bool AutofillClientImpl::IsOffTheRecord() {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

scoped_refptr<network::SharedURLLoaderFactory>
AutofillClientImpl::GetURLLoaderFactory() {
  return web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

autofill::AutofillDownloadManager* AutofillClientImpl::GetDownloadManager() {
  if (!download_manager_) {
    // Lazy initialization to avoid virtual function calls in the constructor.
    download_manager_ = std::make_unique<autofill::AutofillDownloadManager>(
        this, GetChannel(), GetLogManager());
  }
  return download_manager_.get();
}

autofill::PersonalDataManager* AutofillClientImpl::GetPersonalDataManager() {
  NOTREACHED();
  return nullptr;
}

autofill::AutocompleteHistoryManager*
AutofillClientImpl::GetAutocompleteHistoryManager() {
  NOTREACHED();
  return nullptr;
}

PrefService* AutofillClientImpl::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* AutofillClientImpl::GetPrefs() const {
  NOTREACHED();
  return nullptr;
}

syncer::SyncService* AutofillClientImpl::GetSyncService() {
  NOTREACHED();
  return nullptr;
}

signin::IdentityManager* AutofillClientImpl::GetIdentityManager() {
  NOTREACHED();
  return nullptr;
}

autofill::FormDataImporter* AutofillClientImpl::GetFormDataImporter() {
  NOTREACHED();
  return nullptr;
}

autofill::payments::PaymentsClient* AutofillClientImpl::GetPaymentsClient() {
  NOTREACHED();
  return nullptr;
}

autofill::StrikeDatabase* AutofillClientImpl::GetStrikeDatabase() {
  NOTREACHED();
  return nullptr;
}

ukm::UkmRecorder* AutofillClientImpl::GetUkmRecorder() {
  // TODO(crbug.com/1181141): Enable the autofill UKM.
  return nullptr;
}

ukm::SourceId AutofillClientImpl::GetUkmSourceId() {
  // TODO(crbug.com/1181141): Enable the autofill UKM.
  return ukm::kInvalidSourceId;
}

autofill::AddressNormalizer* AutofillClientImpl::GetAddressNormalizer() {
  NOTREACHED();
  return nullptr;
}

const GURL& AutofillClientImpl::GetLastCommittedPrimaryMainFrameURL() const {
  NOTREACHED();
  return GURL::EmptyGURL();
}

url::Origin AutofillClientImpl::GetLastCommittedPrimaryMainFrameOrigin() const {
  NOTREACHED();
  return url::Origin();
}

security_state::SecurityLevel
AutofillClientImpl::GetSecurityLevelForUmaHistograms() {
  NOTREACHED();
  return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;
}

const translate::LanguageState* AutofillClientImpl::GetLanguageState() {
  return nullptr;
}

translate::TranslateDriver* AutofillClientImpl::GetTranslateDriver() {
  // The TranslateDriver is used by AutofillManager to observe the page language
  // and run the type-prediction heuristics with language-dependent regexps.
  auto* translate_client = TranslateClientImpl::FromWebContents(web_contents());
  if (translate_client)
    return translate_client->translate_driver();
  return nullptr;
}

void AutofillClientImpl::ShowAutofillSettings(autofill::PopupType popup_type) {
  NOTREACHED();
}

void AutofillClientImpl::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  NOTREACHED();
}

void AutofillClientImpl::OnUnmaskVerificationResult(PaymentsRpcResult result) {
  NOTREACHED();
}

#if !BUILDFLAG(IS_ANDROID)
std::vector<std::string>
AutofillClientImpl::GetAllowedMerchantsForVirtualCards() {
  NOTREACHED();
  return std::vector<std::string>();
}

std::vector<std::string>
AutofillClientImpl::GetAllowedBinRangesForVirtualCards() {
  NOTREACHED();
  return std::vector<std::string>();
}

void AutofillClientImpl::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  NOTREACHED();
}

void AutofillClientImpl::ConfirmMigrateLocalCardToCloud(
    const autofill::LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<autofill::MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  NOTREACHED();
}

void AutofillClientImpl::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const std::u16string& tip_message,
    const std::vector<autofill::MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {
  NOTREACHED();
}

void AutofillClientImpl::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {
  NOTREACHED();
}

void AutofillClientImpl::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {
  NOTREACHED();
}

void AutofillClientImpl::UpdateWebauthnOfferDialogWithError() {
  NOTREACHED();
}

bool AutofillClientImpl::CloseWebauthnDialog() {
  NOTREACHED();
  return false;
}

void AutofillClientImpl::ConfirmSaveUpiIdLocally(
    const std::string& upi_id,
    base::OnceCallback<void(bool user_decision)> callback) {
  NOTREACHED();
}

void AutofillClientImpl::OfferVirtualCardOptions(
    const std::vector<autofill::CreditCard*>& candidates,
    base::OnceCallback<void(const std::string&)> callback) {
  NOTREACHED();
}

#else  // !BUILDFLAG(IS_ANDROID)
void AutofillClientImpl::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  NOTREACHED();
}

void AutofillClientImpl::ConfirmExpirationDateFixFlow(
    const autofill::CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  NOTREACHED();
}
#endif

void AutofillClientImpl::ConfirmSaveCreditCardLocally(
    const autofill::CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  NOTREACHED();
}

void AutofillClientImpl::ConfirmSaveCreditCardToCloud(
    const autofill::CreditCard& card,
    const autofill::LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  NOTREACHED();
}

void AutofillClientImpl::CreditCardUploadCompleted(bool card_saved) {
  NOTREACHED();
}

void AutofillClientImpl::ConfirmCreditCardFillAssist(
    const autofill::CreditCard& card,
    base::OnceClosure callback) {
  NOTREACHED();
}

void AutofillClientImpl::ConfirmSaveAddressProfile(
    const autofill::AutofillProfile& profile,
    const autofill::AutofillProfile* original_profile,
    SaveAddressProfilePromptOptions options,
    AddressProfileSavePromptCallback callback) {
  NOTREACHED();
}

bool AutofillClientImpl::HasCreditCardScanFeature() {
  NOTREACHED();
  return false;
}

void AutofillClientImpl::ScanCreditCard(CreditCardScanCallback callback) {
  NOTREACHED();
}

bool AutofillClientImpl::IsTouchToFillCreditCardSupported() {
  return false;
}

bool AutofillClientImpl::ShowTouchToFillCreditCard(
    base::WeakPtr<autofill::TouchToFillDelegate> delegate,
    base::span<const autofill::CreditCard> cards_to_suggest) {
  NOTREACHED();
  return false;
}

void AutofillClientImpl::HideTouchToFillCreditCard() {
  NOTREACHED();
}

void AutofillClientImpl::ShowAutofillPopup(
    const autofill::AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
  NOTREACHED();
}

void AutofillClientImpl::UpdateAutofillPopupDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
  NOTREACHED();
}

void AutofillClientImpl::HideAutofillPopup(autofill::PopupHidingReason reason) {
  // This is invoked on the user moving away from an autofill context (e.g., a
  // navigation finishing or a tab being hidden). As all showing/hiding of
  // autofill UI in WebLayer is driven by the system, there is no action to
  // take.
}

std::vector<autofill::Suggestion> AutofillClientImpl::GetPopupSuggestions()
    const {
  NOTIMPLEMENTED();
  return {};
}

void AutofillClientImpl::PinPopupView() {
  NOTIMPLEMENTED();
}

autofill::AutofillClient::PopupOpenArgs AutofillClientImpl::GetReopenPopupArgs()
    const {
  NOTIMPLEMENTED();
  return {};
}

void AutofillClientImpl::UpdatePopup(
    const std::vector<autofill::Suggestion>& suggestions,
    autofill::PopupType popup_type) {
  NOTREACHED();
}

bool AutofillClientImpl::IsAutocompleteEnabled() const {
  NOTREACHED();
  return false;
}

bool AutofillClientImpl::IsPasswordManagerEnabled() {
  // This function is currently only used by the BrowserAutofillManager,
  // but not by the AndroidAutofillManager. See crbug.com/1293341 for context.
  NOTREACHED();
  return false;
}

void AutofillClientImpl::PropagateAutofillPredictions(
    autofill::AutofillDriver* driver,
    const std::vector<autofill::FormStructure*>& forms) {
  NOTREACHED();
}

void AutofillClientImpl::DidFillOrPreviewForm(
    autofill::mojom::RendererFormDataAction action,
    autofill::AutofillTriggerSource trigger_source,
    bool is_refill) {
  NOTREACHED();
}

void AutofillClientImpl::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {
  NOTREACHED();
}

bool AutofillClientImpl::IsContextSecure() const {
  NOTREACHED();
  return false;
}

void AutofillClientImpl::ExecuteCommand(int id) {
  NOTREACHED();
}

void AutofillClientImpl::OpenPromoCodeOfferDetailsURL(const GURL& url) {
  NOTREACHED();
}

autofill::FormInteractionsFlowId
AutofillClientImpl::GetCurrentFormInteractionsFlowId() {
  // Currently not in use here. See `ChromeAutofillClient` for a proper
  // implementation.
  return {};
}

void AutofillClientImpl::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  NOTREACHED();
}

AutofillClientImpl::AutofillClientImpl(content::WebContents* web_contents)
    : autofill::ContentAutofillClient(
          web_contents,
          base::BindRepeating(&autofill::AndroidDriverInitHook, this)),
      content::WebContentsObserver(web_contents) {}

}  // namespace weblayer
