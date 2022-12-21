// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IBooleanCallback;
import org.chromium.weblayer_private.interfaces.ICookieChangedCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IStringCallback;

interface ICookieManager {
  void setCookie(in String url, in String value, in IBooleanCallback callback) = 0;

  void getCookie(in String url, in IStringCallback callback) = 1;

  IObjectWrapper addCookieChangedCallback(in String url, in String name, ICookieChangedCallbackClient callback) = 2;

  // Added in 101.
  void getResponseCookies(in String url, in IObjectWrapper callback) = 3;

}
