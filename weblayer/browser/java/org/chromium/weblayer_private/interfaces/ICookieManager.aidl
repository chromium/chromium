// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.ICookieChangedCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface ICookieManager {
  boolean setCookie(in String url, in String value, in IObjectWrapper callback) = 0;

  void getCookie(in String url, in IObjectWrapper callback) = 1;

  IObjectWrapper addCookieChangedCallback(in String url, in String name, ICookieChangedCallbackClient callback) = 2;
}
