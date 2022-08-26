// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import android.os.Bundle;
import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegateClient;
import org.chromium.browserfragment.interfaces.IBooleanCallback;
import org.chromium.browserfragment.interfaces.ITabListObserverDelegate;
import org.chromium.browserfragment.interfaces.ITabProxy;
import org.chromium.browserfragment.interfaces.ITabCallback;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

// Next value: 20
oneway interface IBrowserFragmentDelegate {
    void setClient(in IBrowserFragmentDelegateClient client) = 1;

    void attachViewHierarchy(in IBinder hostToken) = 2;
    void resizeView(in int width, in int height) = 3;

    // Fragment events.
    void onCreate(in Bundle savedInstanceState) = 4;
    void onAttach() = 5;
    // This is used only for the in-process service.
    void onAttachWithContext(IObjectWrapper context) = 19;

    void onDestroy() = 6;
    void onDetach() = 7;
    void onStart() = 8;
    void onStop() = 9;
    void onResume() = 10;
    void onPause() = 11;

    // In process operations.
    void retrieveContentViewRenderView() = 12;

    // Tab operations.
    void getActiveTab(ITabCallback callback) = 14;
    void setTabListObserverDelegate(ITabListObserverDelegate tabListObserverDelegate) = 15;
    void tryNavigateBack(IBooleanCallback callback) = 17;
    void createTab(ITabCallback callback) = 18;
}