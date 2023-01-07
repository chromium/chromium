// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_APPLICATION_INFO_HELPER_H_
#define WEBLAYER_BROWSER_ANDROID_APPLICATION_INFO_HELPER_H_

#include <string>

namespace weblayer {

// Looks for a metadata tag with name |key| in the application's manifest, and
// returns its value if found, or |default_value| otherwise.
bool GetApplicationMetadataAsBoolean(const std::string& key,
                                     bool default_value);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_APPLICATION_INFO_HELPER_H_
