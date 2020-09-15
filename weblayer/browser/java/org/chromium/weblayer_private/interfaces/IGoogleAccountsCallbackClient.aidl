// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IGoogleAccountsCallbackClient {
  void onGoogleAccountsRequest(int serviceType, in String email, in String continueUrl, boolean isSameTab) = 0;
  String getGaiaId() = 1;

  // Since 87
  String getFullName() = 2;
  // avatarLoadedWrapper is a ValueCallback<Bitmap> that updates the profile icon when run.
  void getAvatar(int desiredSize, in IObjectWrapper avatarLoadedWrapper) = 3;
}
