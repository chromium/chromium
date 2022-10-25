// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.IStringCallback;

oneway interface ICookieManagerDelegate {
    void setCookie(String uri, String value, IBooleanCallback callback) = 1;
    void getCookie(String uri, IStringCallback callback) = 2;
}
