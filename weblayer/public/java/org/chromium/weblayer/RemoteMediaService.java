// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Intent;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.RemoteMediaServiceConstants;

/**
 * A foreground {@link Service} for Presentation API and Remote Playback API.
 *
 * Like {@link MediaSessionService}, this class is associated with a notification for an ongoing
 * media session. The difference is that the media for this service is played back on a remote
 * device, i.e. casting. This class can be considered an implementation detail of WebLayer.
 *
 * In order to set the Cast application (optional but recommended), the client should add the
 * following to its manifest:
 *
 *    <meta-data
 *        android:name="org.chromium.content.browser.REMOTE_PLAYBACK_APP_ID"
 *        android:value="$APP_ID"/>
 *
 * Where $APP_ID is the value assigned to your app by the Google Cast SDK Developer Console. If
 * the cast application ID is not set, the app will appear as "Default Media Receiver" in the
 * notification, device selection dialog, etc.
 *
 * @since 88
 */
class RemoteMediaService extends MediaPlaybackBaseService {
    private int mId;

    @Override
    void forwardStartCommandToImpl(@NonNull WebLayer webLayer, Intent intent)
            throws RemoteException {
        mId = intent.getIntExtra(RemoteMediaServiceConstants.NOTIFICATION_ID_KEY, 0);
        if (mId == 0) throw new RuntimeException("Invalid RemoteMediaService notification id");

        webLayer.getImpl().onRemoteMediaServiceStarted(ObjectWrapper.wrap(this), intent);
    }

    @Override
    void forwardDestroyToImpl() throws RemoteException {
        getWebLayer().getImpl().onRemoteMediaServiceDestroyed(mId);
    }
}
