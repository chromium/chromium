// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import android.view.SurfaceControlViewHost.SurfacePackage;
import org.chromium.webengine.interfaces.IProfileManagerDelegate;

oneway interface IWebSandboxCallback {
    void onBrowserProcessInitialized(in IProfileManagerDelegate delegate) = 1;
    void onBrowserProcessInitializationFailure() = 2;
}