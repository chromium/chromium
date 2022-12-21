// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import android.view.SurfaceControlViewHost.SurfacePackage;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

oneway interface IWebFragmentEventsDelegateClient {
    void onSurfacePackageReady(in SurfacePackage surfacePackage) = 1;

    // Pre-U/T -devices cannot create an out-of-process Service with privileges needed
    // to run the Browser process.
    // On those devices we run BrowserFragment in-process but with the same API.
    // The ObjectWrapper is only needed to pass the View-object (ContentViewRenderView)
    // to the connecting client as the SurfaceControlViewHost is also not available on
    // older versions.
    void onContentViewRenderViewReady(in IObjectWrapper contentViewRenderView) = 2;
}
