// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import android.os.Bundle;

import org.chromium.webengine.interfaces.IWebFragmentEventsDelegateClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

// Next value: 15
oneway interface IWebFragmentEventsDelegate {
    void setClient(in IWebFragmentEventsDelegateClient client) = 1;

    // Fragment events.
    void onCreate() = 2;
    void onStart() = 3;
    void onAttach() = 4;
    void onDetach() = 5;
    void onPause() = 6;
    void onStop() = 7;
    void onResume() = 8;
    void onDestroy() = 9;

    // Fragment operations.
    void resizeView(in int width, in int height) = 10;
    void setMinimumSurfaceSize(int width, int height) = 11;

    // Out of process operations.
    void attachViewHierarchy(in IBinder hostToken) = 12;

    // In process operations.
    void onAttachWithContext(IObjectWrapper context) = 13;
    void retrieveContentViewRenderView() = 14;
}
