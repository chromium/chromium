// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_STUB_AUTOFILL_PROVIDER_H_
#define WEBLAYER_TEST_STUB_AUTOFILL_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "components/android_autofill/browser/test_autofill_provider.h"
#include "content/public/browser/web_contents.h"

namespace weblayer {

// A stub AutofillProvider implementation that is used in cross-platform
// integration tests of renderer-side autofill detection and communication to
// the browser.
class StubAutofillProvider : public autofill::TestAutofillProvider {
 public:
  // WebContents takes the ownership of StubAutofillProvider.
  explicit StubAutofillProvider(
      content::WebContents* web_contents,
      const base::RepeatingCallback<void(const autofill::FormData&)>&
          on_received_form_data);

  StubAutofillProvider(const StubAutofillProvider&) = delete;
  StubAutofillProvider& operator=(const StubAutofillProvider&) = delete;

  ~StubAutofillProvider() override;

  // AutofillProvider:
  void OnAskForValuesToFill(
      autofill::AndroidAutofillManager* manager,
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const gfx::RectF& bounding_box,
      autofill::
          AutoselectFirstSuggestion /*unused_autoselect_first_suggestion*/,
      autofill::FormElementWasClicked /*unused_form_element_was_clicked*/)
      override;

 private:
  base::RepeatingCallback<void(const autofill::FormData&)>
      on_received_form_data_;
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_STUB_AUTOFILL_PROVIDER_H_
