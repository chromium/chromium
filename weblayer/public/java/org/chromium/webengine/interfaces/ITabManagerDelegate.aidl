// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.ITabProxy;
import org.chromium.webengine.interfaces.ITabCallback;

oneway interface ITabManagerDelegate {
    void getActiveTab(ITabCallback callback) = 14;
    void tryNavigateBack(IBooleanCallback callback) = 17;
    void createTab(ITabCallback callback) = 18;
}