// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.INavigationParams;

oneway interface INavigationObserverDelegate {
    void notifyNavigationStarted(in INavigationParams navigation) = 1;
    void notifyNavigationCompleted(in INavigationParams navigation) = 2;
    void notifyNavigationFailed(in INavigationParams navigation) = 3;
    void notifyLoadProgressChanged(double progress) = 4;
    void notifyNavigationRedirected(in INavigationParams navigation) = 5;
}
