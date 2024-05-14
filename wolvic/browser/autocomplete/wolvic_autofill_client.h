// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_AUTOFILL_CLIENT_H_
#define WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/filling_product.h"

namespace payments {
class PaymentsClient;
}  // namespace payments

namespace wolvic {

class WolvicAutofillClient : public autofill::ContentAutofillClient {
 public:
  // Creates a new WolvicAutofillClient for the given `web_contents` if no
  // ContentAutofillClient is associated with the `web_contents` yet. Otherwise,
  // it's a no-op.
  static void CreateForWebContents(content::WebContents* web_contents);

  WolvicAutofillClient(const WolvicAutofillClient&) = delete;
  WolvicAutofillClient& operator=(const WolvicAutofillClient&) = delete;
  ~WolvicAutofillClient() override;

  // autofill::AutofillClient:
  bool IsOffTheRecord() const override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() override;
  autofill::AutofillCrowdsourcingManager* GetCrowdsourcingManager() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  autofill::AutocompleteHistoryManager*
  GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::payments::PaymentsAutofillClient* GetPaymentsAutofillClient()
      override;
  autofill::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  void ShowAutofillSettings(
      autofill::FillingProduct main_filling_product) override;
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const autofill::CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
  void ConfirmSaveCreditCardLocally(
      const autofill::CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;
  void ConfirmSaveCreditCardToCloud(
      const autofill::CreditCard& card,
      const autofill::LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void ConfirmCreditCardFillAssist(const autofill::CreditCard& card,
                                   base::OnceClosure callback) override;
  void ShowEditAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
  void ConfirmSaveAddressProfile(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() const override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<autofill::TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillPopup(
      const PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      base::span<const autofill::SelectOption> datalist) override;
  std::vector<autofill::Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  void UpdatePopup(
      const std::vector<autofill::Suggestion>& suggestions,
      autofill::FillingProduct main_filling_product,
      autofill::AutofillSuggestionTriggerSource trigger_source) override;
  void HideAutofillPopup(autofill::PopupHidingReason reason) override;

  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(
      autofill::mojom::ActionPersistence action_persistence,
      autofill::AutofillTriggerSource trigger_source,
      bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  autofill::FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;

  // autofill::ContentAutofillClient:
  std::unique_ptr<autofill::AutofillManager> CreateManager(
      base::PassKey<autofill::ContentAutofillDriver> pass_key,
      autofill::ContentAutofillDriver& driver) override;

  void OnLoginSelected(JNIEnv* env, jint index);

 protected:
  explicit WolvicAutofillClient(content::WebContents* web_contents);

 private:
  content::WebContents* web_contents() const { return web_contents_; }
  void CreatJavaArrayFromSuggestions(JNIEnv* env);

  // These members are initialized lazily in their respective getters.
  // Therefore, do not access the members directly.
  std::unique_ptr<autofill::AutofillCrowdsourcingManager> crowdsourcing_manager_;

  autofill::FormInteractionsFlowId flow_id_{};
  base::Time flow_id_date_;
  raw_ptr<content::WebContents> web_contents_;
  std::vector<autofill::Suggestion> suggestions_;
  autofill::AutofillSuggestionTriggerSource trigger_source_{
      autofill::AutofillSuggestionTriggerSource::kUnspecified};
  base::WeakPtr<autofill::AutofillPopupDelegate> delegate_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_AUTOFILL_CLIENT_H_
