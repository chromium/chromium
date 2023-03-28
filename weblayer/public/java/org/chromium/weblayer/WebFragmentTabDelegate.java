// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.webengine.interfaces.ITabObserverDelegate;

/**
 * This class acts as a proxy between the Tab events happening in
 * weblayer and the TabObserverDelegate in webengine.
 */
class WebFragmentTabDelegate extends TabCallback {
    private ITabObserverDelegate mTabObserver;

    void setObserver(ITabObserverDelegate observer) {
        mTabObserver = observer;
    }

    @Override
    public void onVisibleUriChanged(@NonNull Uri uri) {
        maybeRunOnTabObserver(observer -> { observer.notifyVisibleUriChanged(uri.toString()); });
    }

    @Override
    public void onRenderProcessGone() {
        maybeRunOnTabObserver(observer -> { observer.notifyRenderProcessGone(); });
    }

    @Override
    public void onTitleUpdated(@NonNull String title) {
        maybeRunOnTabObserver(observer -> { observer.notifyTitleUpdated(title); });
    }

    private interface OnTabObserverCallback {
        void run(ITabObserverDelegate tabObserver) throws RemoteException;
    }

    private void maybeRunOnTabObserver(OnTabObserverCallback callback) {
        if (mTabObserver != null) {
            try {
                callback.run(mTabObserver);
            } catch (RemoteException e) {
            }
        }
    }
}