// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/url_bar/autocomplete_scheme_classifier_impl.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/public/common/url_constants.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "weblayer/browser/java/jni/AutocompleteSchemeClassifierImpl_jni.h"
#endif

namespace weblayer {

#if defined(OS_ANDROID)
static jlong JNI_AutocompleteSchemeClassifierImpl_CreateAutocompleteClassifier(
    JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new AutocompleteSchemeClassifierImpl());
}

static void JNI_AutocompleteSchemeClassifierImpl_DeleteAutocompleteClassifier(
    JNIEnv* env,
    jlong autocomplete_scheme_classifier_impl) {
  delete reinterpret_cast<AutocompleteSchemeClassifierImpl*>(
      autocomplete_scheme_classifier_impl);
}
#endif

metrics::OmniboxInputType
AutocompleteSchemeClassifierImpl::GetInputTypeForScheme(
    const std::string& scheme) const {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));

  // Check against an allowlist of schemes.
  const char* kKnownURLSchemes[] = {
      url::kHttpScheme,       url::kHttpsScheme,
      url::kWsScheme,         url::kWssScheme,
      url::kFileScheme,       url::kAboutScheme,
      url::kFtpScheme,        url::kBlobScheme,
      url::kFileSystemScheme, content::kViewSourceScheme,
      url::kJavaScriptScheme};

  for (const char* known_scheme : kKnownURLSchemes) {
    if (scheme == known_scheme)
      return metrics::OmniboxInputType::URL;
  }

  return metrics::OmniboxInputType::EMPTY;
}

}  // namespace weblayer
