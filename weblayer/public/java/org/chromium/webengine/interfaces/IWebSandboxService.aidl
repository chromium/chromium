// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.IStringCallback;
import org.chromium.webengine.interfaces.IWebEngineParams;
import org.chromium.webengine.interfaces.IWebEngineDelegateClient;
import org.chromium.webengine.interfaces.IWebSandboxCallback;

oneway interface IWebSandboxService {
    void isAvailable(IBooleanCallback callback) = 1;
    void getVersion(IStringCallback callback) = 2;
    void getProviderPackageName(IStringCallback callback) = 3;

    void initializeBrowserProcess(in IWebSandboxCallback callback) = 4;
    void createWebEngineDelegate(in IWebEngineParams params, IWebEngineDelegateClient fragmentClient) = 5;

    void setRemoteDebuggingEnabled(in boolean enabled) = 6;
}
