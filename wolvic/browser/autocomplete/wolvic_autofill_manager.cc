// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/autocomplete/wolvic_autofill_manager.h"

#include "components/autofill/core/browser/autofill_manager.h"

namespace wolvic {

WolvicAutofillManager::WolvicAutofillManager(autofill::AutofillDriver* driver,
                                             autofill::AutofillClient* client)
    : autofill::AutofillManager(driver, client) {}

WolvicAutofillManager::~WolvicAutofillManager() = default;

base::WeakPtr<autofill::AutofillManager> WolvicAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WolvicAutofillManager::ShouldClearPreviewedForm() {
  return false;
}

bool WolvicAutofillManager::ShouldParseForms() {
  return false;
}

}  // namespace wolvic
