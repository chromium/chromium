// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import android.view.SurfaceControlViewHost.SurfacePackage;

oneway interface IBrowserSandboxCallback {
    void onBrowserProcessInitialized() = 1;
}