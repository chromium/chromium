// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_AUTOFILL_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_AUTOFILL_CLIENT_IMPL_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace weblayer {

// A minimal implementation of autofill::AutofillClient to satisfy the minor
// touchpoints between the autofill implementation and its client that get
// exercised within the WebLayer autofill flow.
class AutofillClientImpl : public autofill::ContentAutofillClient,
                           public content::WebContentsObserver {
 public:
  static AutofillClientImpl* FromWebContents(
      content::WebContents* web_contents) {
    return static_cast<AutofillClientImpl*>(
        ContentAutofillClient::FromWebContents(web_contents));
  }

  static void CreateForWebContents(content::WebContents* contents) {
    DCHECK(contents);
    if (!ContentAutofillClient::FromWebContents(contents)) {
      contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new AutofillClientImpl(contents)));
    }
  }

  AutofillClientImpl(const AutofillClientImpl&) = delete;
  AutofillClientImpl& operator=(const AutofillClientImpl&) = delete;

  ~AutofillClientImpl() override;

  // AutofillClient:
  bool IsOffTheRecord() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  autofill::AutofillDownloadManager* GetDownloadManager() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager()
      override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::payments::PaymentsClient* GetPaymentsClient() override;
  autofill::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;

  void ShowAutofillSettings(autofill::PopupType popup_type) override;
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;

#if !BUILDFLAG(IS_ANDROID)
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
      const std::u16string& tip_message,
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
#else  // !BUILDFLAG(IS_ANDROID)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const autofill::CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
#endif  // !BUILDFLAG(IS_ANDROID)
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
  void ConfirmSaveAddressProfile(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool IsTouchToFillCreditCardSupported() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<autofill::TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillPopup(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<std::u16string>& values,
      const std::vector<std::u16string>& labels) override;
  std::vector<autofill::Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  autofill::AutofillClient::PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<autofill::Suggestion>& suggestions,
                   autofill::PopupType popup_type) override;
  void HideAutofillPopup(autofill::PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void PropagateAutofillPredictions(
      autofill::AutofillDriver* driver,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewForm(autofill::mojom::RendererFormDataAction action,
                            autofill::AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  void ExecuteCommand(autofill::Suggestion::FrontendId id) override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  autofill::FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

 private:
  explicit AutofillClientImpl(content::WebContents* web_contents);

  std::unique_ptr<autofill::AutofillDownloadManager> download_manager_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_AUTOFILL_CLIENT_IMPL_H_
