// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/autofill_client_impl.h"

#include "components/autofill/core/browser/ui/suggestion.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"

namespace weblayer {

AutofillClientImpl::~AutofillClientImpl() = default;

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
  NOTREACHED();
  return nullptr;
}

ukm::SourceId AutofillClientImpl::GetUkmSourceId() {
  NOTREACHED();
  return ukm::kInvalidSourceId;
}

autofill::AddressNormalizer* AutofillClientImpl::GetAddressNormalizer() {
  NOTREACHED();
  return nullptr;
}

const GURL& AutofillClientImpl::GetLastCommittedURL() {
  NOTREACHED();
  return GURL::EmptyGURL();
}

security_state::SecurityLevel
AutofillClientImpl::GetSecurityLevelForUmaHistograms() {
  NOTREACHED();
  return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;
}

const translate::LanguageState* AutofillClientImpl::GetLanguageState() {
  return nullptr;
}

void AutofillClientImpl::ShowAutofillSettings(bool show_credit_card_settings) {
  NOTREACHED();
}

void AutofillClientImpl::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  NOTREACHED();
}

void AutofillClientImpl::OnUnmaskVerificationResult(PaymentsRpcResult result) {
  NOTREACHED();
}

#if !defined(OS_ANDROID)
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
    const base::string16& tip_message,
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

#else  // defined(OS_ANDROID)
void AutofillClientImpl::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const base::string16&)> callback) {
  NOTREACHED();
}

void AutofillClientImpl::ConfirmExpirationDateFixFlow(
    const autofill::CreditCard& card,
    base::OnceCallback<void(const base::string16&, const base::string16&)>
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

bool AutofillClientImpl::HasCreditCardScanFeature() {
  NOTREACHED();
  return false;
}

void AutofillClientImpl::ScanCreditCard(CreditCardScanCallback callback) {
  NOTREACHED();
}

void AutofillClientImpl::ShowAutofillPopup(
    const autofill::AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
  NOTREACHED();
}

void AutofillClientImpl::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  NOTREACHED();
}

void AutofillClientImpl::HideAutofillPopup(autofill::PopupHidingReason reason) {
  // This is invoked on the user moving away from an autofill context (e.g., a
  // navigation finishing or a tab being hidden). As all showing/hiding of
  // autofill UI in WebLayer is driven by the system, there is no action to
  // take.
}

base::span<const autofill::Suggestion> AutofillClientImpl::GetPopupSuggestions()
    const {
  NOTIMPLEMENTED();
  return base::span<const autofill::Suggestion>();
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

bool AutofillClientImpl::IsAutocompleteEnabled() {
  NOTREACHED();
  return false;
}

void AutofillClientImpl::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {
  NOTREACHED();
}

void AutofillClientImpl::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
  NOTREACHED();
}

bool AutofillClientImpl::IsContextSecure() {
  NOTREACHED();
  return false;
}

bool AutofillClientImpl::ShouldShowSigninPromo() {
  NOTREACHED();
  return false;
}

bool AutofillClientImpl::AreServerCardsSupported() {
  NOTREACHED();
  return false;
}

void AutofillClientImpl::ExecuteCommand(int id) {
  NOTREACHED();
}

void AutofillClientImpl::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  NOTREACHED();
}

AutofillClientImpl::AutofillClientImpl(content::WebContents* web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutofillClientImpl)

}  // namespace weblayer
