// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * A foreground {@link Service} for the Web MediaSession API.
 * This class is a thin wrapper that forwards lifecycle events to the WebLayer implementation, which
 * in turn manages a system notification and {@link MediaSession}. This service will be in the
 * foreground when the MediaSession is active.
 */
public class MediaSessionService extends MediaPlaybackBaseService {
    // A helper to automatically pause the media session when a user removes headphones.
    private BroadcastReceiver mAudioBecomingNoisyReceiver;

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mAudioBecomingNoisyReceiver != null) {
            unregisterReceiver(mAudioBecomingNoisyReceiver);
        }
    }

    @Override
    void init() {
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
        ContextUtils.registerProtectedBroadcastReceiver(this, mAudioBecomingNoisyReceiver, filter);
    }

    @Override
    void forwardStartCommandToImpl(@NonNull WebLayer webLayer, Intent intent)
            throws RemoteException {
        webLayer.getImpl().onMediaSessionServiceStarted(ObjectWrapper.wrap(this), intent);
    }

    @Override
    void forwardDestroyToImpl() throws RemoteException {
        getWebLayer().getImpl().onMediaSessionServiceDestroyed();
    }
}
