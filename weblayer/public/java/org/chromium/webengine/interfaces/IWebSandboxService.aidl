// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IWebFragmentDelegate;
import org.chromium.webengine.interfaces.IWebSandboxCallback;
import org.chromium.webengine.interfaces.IFragmentParams;

interface IWebSandboxService {
    void initializeBrowserProcess(in IWebSandboxCallback callback) = 1;

    IWebFragmentDelegate createFragmentDelegate(in IFragmentParams params) = 2;

    void setRemoteDebuggingEnabled(in boolean enabled) = 3;
}