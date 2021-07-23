// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/stub_autofill_provider.h"

namespace weblayer {

StubAutofillProvider::StubAutofillProvider(
    content::WebContents* web_contents,
    const base::RepeatingCallback<void(const autofill::FormData&)>&
        on_received_form_data)
    : autofill::TestAutofillProvider(web_contents),
      on_received_form_data_(on_received_form_data) {}

StubAutofillProvider::~StubAutofillProvider() = default;

void StubAutofillProvider::OnAskForValuesToFill(
    autofill::AndroidAutofillManager* manager,
    int32_t id,
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool /*unused_autoselect_first_suggestion*/) {
  on_received_form_data_.Run(form);
}

}  // namespace weblayer
