// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/android/gurl_test_init.h"
#include "url/url_util.h"

namespace url {
// Registers enough to have //url parsing work as expected.
// Does not directly reference //content or //chrome to save on compile times.
void RegisterSchemesForRobolectric() {
  // Schemes from content/common/url_schemes.cc:
  url::AddStandardScheme("chrome", SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-untrusted", SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-error", SCHEME_WITH_HOST);
  url::AddNoAccessScheme("chrome-error");

  // Schemes from chrome/common/chrome_content_client.cc:
  url::AddStandardScheme("isolated-app", SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-native", SCHEME_WITH_HOST);
  url::AddNoAccessScheme("chrome-native");
  url::AddStandardScheme("chrome-search", SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-distiller", SCHEME_WITH_HOST);
  url::AddStandardScheme("android-app", SCHEME_WITH_HOST);
  url::AddLocalScheme("content");
}
}  // namespace url
