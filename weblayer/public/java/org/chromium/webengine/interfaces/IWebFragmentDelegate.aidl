// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import android.os.Bundle;
import org.chromium.webengine.interfaces.IWebFragmentDelegateClient;
import org.chromium.webengine.interfaces.ITabListObserverDelegate;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

// Next value: 21
oneway interface IWebFragmentDelegate {
    void setClient(in IWebFragmentDelegateClient client) = 1;

    void attachViewHierarchy(in IBinder hostToken) = 2;
    void resizeView(in int width, in int height) = 3;

    // Fragment events.
    void onCreate(in Bundle savedInstanceState) = 4;
    void onAttach() = 5;
    // This is used only for the in-process service.
    void onAttachWithContext(IObjectWrapper context) = 19;
    void setMinimumSurfaceSize(int width, int height) = 20;

    void onDestroy() = 6;
    void onDetach() = 7;
    void onStart() = 8;
    void onStop() = 9;
    void onResume() = 10;
    void onPause() = 11;

    // In process operations.
    void retrieveContentViewRenderView() = 12;

    // Tab operations.
    void setTabListObserverDelegate(ITabListObserverDelegate tabListObserverDelegate) = 15;
}