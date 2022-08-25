// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import org.chromium.browserfragment.interfaces.INavigationParams;

oneway interface INavigationObserverDelegate {
    void notifyNavigationStarted(in INavigationParams navigation) = 1;
    void notifyNavigationCompleted(in INavigationParams navigation) = 2;
    void notifyNavigationFailed(in INavigationParams navigation) = 3;
    void notifyLoadProgressChanged(double progress) = 4;
}
