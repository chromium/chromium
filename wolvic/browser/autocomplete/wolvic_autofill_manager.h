// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_AUTOFILL_MANAGER_H_
#define WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_AUTOFILL_MANAGER_H_

#include "components/autofill/core/browser/autofill_manager.h"

namespace wolvic {

class WolvicAutofillManager : public autofill::AutofillManager {
 public:
  explicit WolvicAutofillManager(autofill::AutofillDriver* driver);

  WolvicAutofillManager(const WolvicAutofillManager&) = delete;
  WolvicAutofillManager& operator=(const WolvicAutofillManager&) = delete;

  ~WolvicAutofillManager() override;

  // autofill::AutofillManager:
  base::WeakPtr<AutofillManager> GetWeakPtr() override;
  bool ShouldClearPreviewedForm() override;

  void OnFocusNoLongerOnFormImpl(bool had_interacted_form) override {}

  void OnDidFillAutofillFormDataImpl(const autofill::FormData& form,
                                     const base::TimeTicks timestamp) override {
  }

  void OnDidEndTextFieldEditingImpl() override {}
  void OnHidePopupImpl() override {}
  void OnSelectOrSelectListFieldOptionsDidChangeImpl(
      const autofill::FormData& form) override {}

  void Reset() override {}

  void ReportAutofillWebOTPMetrics(bool used_web_otp) override {}

 protected:
  void OnFormSubmittedImpl(const autofill::FormData& form,
                           bool known_success,
                           autofill::mojom::SubmissionSource source) override {}

  void OnTextFieldDidChangeImpl(const autofill::FormData& form,
                                const autofill::FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override {}

  void OnTextFieldDidScrollImpl(const autofill::FormData& form,
                                const autofill::FormFieldData& field,
                                const gfx::RectF& bounding_box) override {}

  void OnAskForValuesToFillImpl(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const gfx::RectF& bounding_box,
      autofill::AutofillSuggestionTriggerSource trigger_source) override {}

  void OnFocusOnFormFieldImpl(const autofill::FormData& form,
                              const autofill::FormFieldData& field,
                              const gfx::RectF& bounding_box) override {}

  void OnSelectControlDidChangeImpl(const autofill::FormData& form,
                                    const autofill::FormFieldData& field,
                                    const gfx::RectF& bounding_box) override {}

  void OnJavaScriptChangedAutofilledValueImpl(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const std::u16string& old_value) override {}

  bool ShouldParseForms() override;

  void OnBeforeProcessParsedForms() override {}

  void OnFormProcessed(const autofill::FormData& form,
                       const autofill::FormStructure& form_structure) override {
  }

  void OnAfterProcessParsedForms(
      const autofill::DenseSet<autofill::FormType>& form_types) override {}

 private:
  base::WeakPtrFactory<WolvicAutofillManager> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_AUTOFILL_MANAGER_H_
