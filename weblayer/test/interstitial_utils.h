// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_INTERSTITIAL_UTILS_H_
#define WEBLAYER_TEST_INTERSTITIAL_UTILS_H_

namespace weblayer {

class Tab;

// Contains utilities for aiding in testing an embedder's integration of
// WebLayer's interstitial functionality.

// Returns true iff a security interstitial is currently displaying in
// |tab|.
bool IsShowingSecurityInterstitial(Tab* tab);

// Returns true iff an SSL error-related interstitial is currently displaying in
// |tab|.
bool IsShowingSSLInterstitial(Tab* tab);

}  // namespace weblayer

#endif  // WEBLAYER_TEST_INTERSTITIAL_UTILS_H_
