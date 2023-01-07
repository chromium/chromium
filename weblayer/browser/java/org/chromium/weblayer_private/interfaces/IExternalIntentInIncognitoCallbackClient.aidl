// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IExternalIntentInIncognitoCallbackClient {
  // onUserDecisionWrapper is a ValueCallback<Integer>.
  void onExternalIntentInIncognito(in IObjectWrapper onUserDecisionWrapper) = 0;
}
