// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Base class that provides common functionality for media playback services that are associated
 * with an active media session and Android notification.
 */
abstract class MediaPlaybackBaseService extends Service {
    // True when the start command has been forwarded to the impl.
    boolean mStarted;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        if (!WebLayer.hasWebLayerInitializationStarted()) {
            stopSelf();
            return;
        }

        init();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        WebLayer webLayer = getWebLayer();
        if (webLayer == null) {
            stopSelf();
        } else {
            try {
                forwardStartCommandToImpl(webLayer, intent);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
            mStarted = true;
        }

        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (!mStarted) return;

        try {
            forwardDestroyToImpl();
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    /** Called to do initialization when the service is created. */
    void init() {}

    /**
     * Called to forward {@link onStartCommand()} to the WebLayer implementation.
     *
     * @param webLayer the implementation.
     * @param intent the intent that started the service.
     */
    abstract void forwardStartCommandToImpl(@NonNull WebLayer webLayer, Intent intent)
            throws RemoteException;

    /**
     * Called to forward {@link onDestroy()} to the WebLayer implementation.
     *
     * This will only be called if {@link forwardStartCommandToImpl()} was previously called, and
     * there should always be a loaded {@link WebLayer} available via {@link getWebLayer()}.
     */
    abstract void forwardDestroyToImpl() throws RemoteException;

    /** Returns the loaded {@link WebLayer}, or null if none is loaded. */
    @Nullable
    WebLayer getWebLayer() {
        WebLayer webLayer;
        try {
            webLayer = WebLayer.getLoadedWebLayer(getApplication());
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException(e);
        }
        return webLayer;
    }
}
