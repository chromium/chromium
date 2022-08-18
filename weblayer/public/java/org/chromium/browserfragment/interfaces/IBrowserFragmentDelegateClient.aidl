// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import android.view.SurfaceControlViewHost.SurfacePackage;

oneway interface IBrowserFragmentDelegateClient {
    void onSurfacePackageReady(in SurfacePackage surfacePackage) = 1;
    void onStarted(in Bundle instanceState) = 2;
}