// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_AUTOFILL_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_AUTOFILL_CLIENT_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents_user_data.h"

namespace weblayer {

// A minimal implementation of autofill::AutofillClient to satisfy the minor
// touchpoints between the autofill implementation and its client that get
// exercised within the WebLayer autofill flow.
class AutofillClientImpl
    : public autofill::AutofillClient,
      public content::WebContentsUserData<AutofillClientImpl> {
 public:
  ~AutofillClientImpl() override;

  // AutofillClient:
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager()
      override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::payments::PaymentsClient* GetPaymentsClient() override;
  autofill::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedURL() override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;

  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      UnmaskCardReason reason,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;

#if !defined(OS_ANDROID)
  std::vector<std::string> GetAllowedMerchantsForVirtualCards() override;
  std::vector<std::string> GetAllowedBinRangesForVirtualCards() override;

  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      const autofill::LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<autofill::MigratableCreditCard>&
          migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ShowLocalCardMigrationResults(
      const bool has_server_error,
      const base::string16& tip_message,
      const std::vector<autofill::MigratableCreditCard>&
          migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback) override;
  void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback) override;
  void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback) override;
  void UpdateWebauthnOfferDialogWithError() override;
  bool CloseWebauthnDialog() override;
  void ConfirmSaveUpiIdLocally(
      const std::string& upi_id,
      base::OnceCallback<void(bool user_decision)> callback) override;
  void OfferVirtualCardOptions(
      const std::vector<autofill::CreditCard*>& candidates,
      base::OnceCallback<void(const std::string&)> callback) override;
#else  // if defined(OS_ANDROID)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const base::string16&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const autofill::CreditCard& card,
      base::OnceCallback<void(const base::string16&, const base::string16&)>
          callback) override;
#endif
  void ConfirmSaveCreditCardLocally(
      const autofill::CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;
  void ConfirmSaveCreditCardToCloud(
      const autofill::CreditCard& card,
      const autofill::LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(bool card_saved) override;
  void ConfirmCreditCardFillAssist(const autofill::CreditCard& card,
                                   base::OnceClosure callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  void ShowAutofillPopup(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  base::span<const autofill::Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  autofill::AutofillClient::PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<autofill::Suggestion>& suggestions,
                   autofill::PopupType popup_type) override;
  void HideAutofillPopup(autofill::PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() override;
  void ExecuteCommand(int id) override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

 private:
  explicit AutofillClientImpl(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AutofillClientImpl>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AutofillClientImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_AUTOFILL_CLIENT_IMPL_H_
