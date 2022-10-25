// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.INavigationObserverDelegate;

oneway interface ITabNavigationControllerProxy {
    void navigate(in String uri) = 1;
    void goBack() = 2;
    void goForward() = 3;
    void canGoBack(IBooleanCallback callback) = 4;
    void canGoForward(IBooleanCallback callback) = 5;

    void setNavigationObserverDelegate(INavigationObserverDelegate tabNavigationDelegate) = 6;
}