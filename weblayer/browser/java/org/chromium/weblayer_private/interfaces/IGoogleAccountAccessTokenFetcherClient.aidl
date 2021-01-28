// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IGoogleAccountAccessTokenFetcherClient {
  // scopesWrapper is a Set<String>, and onTokenFetchedWrapper is a ValueCallback<String>.
  void fetchAccessToken(in IObjectWrapper scopesWrapper, in IObjectWrapper onTokenFetchedWrapper) = 0;
}
