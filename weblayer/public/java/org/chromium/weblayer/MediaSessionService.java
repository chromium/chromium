// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * A foreground {@link Service} for the Web MediaSession API.
 * This class is a thin wrapper that forwards lifecycle events to the WebLayer implementation, which
 * in turn manages a system notification and {@link MediaSession}. This service will be in the
 * foreground when the MediaSession is active.
 * @since 85
 */
public class MediaSessionService extends Service {
    // A helper to automatically pause the media session when a user removes headphones.
    private BroadcastReceiver mAudioBecomingNoisyReceiver;

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

        mAudioBecomingNoisyReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (!AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction())) {
                    return;
                }

                Intent i = new Intent(getApplication(), MediaSessionService.class);
                i.setAction(intent.getAction());
                getApplication().startService(i);
            }
        };

        IntentFilter filter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        registerReceiver(mAudioBecomingNoisyReceiver, filter);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mAudioBecomingNoisyReceiver == null) return;

        try {
            getWebLayer().getImpl().onMediaSessionServiceDestroyed();
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }

        unregisterReceiver(mAudioBecomingNoisyReceiver);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        try {
            WebLayer webLayer = getWebLayer();
            if (webLayer == null) {
                stopSelf();
            } else {
                webLayer.getImpl().onMediaSessionServiceStarted(ObjectWrapper.wrap(this), intent);
            }
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }

        return START_NOT_STICKY;
    }

    private WebLayer getWebLayer() {
        WebLayer webLayer;
        try {
            webLayer = WebLayer.getLoadedWebLayer(getApplication());
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException(e);
        }
        return webLayer;
    }
}
