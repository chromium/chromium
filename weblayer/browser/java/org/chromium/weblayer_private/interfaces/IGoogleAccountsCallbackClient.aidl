// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

interface IGoogleAccountsCallbackClient {
  void onGoogleAccountsRequest(int serviceType, in String email, in String continueUrl, boolean isSameTab) = 0;
  String getGaiaId() = 1;
}
