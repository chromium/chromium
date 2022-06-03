// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_URL_BAR_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_
#define WEBLAYER_BROWSER_URL_BAR_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_

#include "components/omnibox/browser/autocomplete_scheme_classifier.h"

namespace weblayer {
class AutocompleteSchemeClassifierImpl : public AutocompleteSchemeClassifier {
 public:
  AutocompleteSchemeClassifierImpl() = default;
  // AutocompleteInputSchemeChecker:
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override;
  ~AutocompleteSchemeClassifierImpl() override = default;
  AutocompleteSchemeClassifierImpl(const AutocompleteSchemeClassifierImpl&) =
      delete;
  AutocompleteSchemeClassifierImpl& operator=(
      const AutocompleteSchemeClassifierImpl&) = delete;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_URL_BAR_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_
