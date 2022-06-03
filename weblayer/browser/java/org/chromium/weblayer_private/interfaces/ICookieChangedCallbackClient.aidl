// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

interface ICookieChangedCallbackClient {
  void onCookieChanged(in String cookie, int cause);
}
