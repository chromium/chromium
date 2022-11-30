// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IUserIdentityCallbackClient {
  String getEmail() = 0;
  String getFullName() = 1;
  // avatarLoadedWrapper is a ValueCallback<Bitmap> that updates the profile icon when run.
  void getAvatar(int desiredSize, in IObjectWrapper avatarLoadedWrapper) = 2;
}
