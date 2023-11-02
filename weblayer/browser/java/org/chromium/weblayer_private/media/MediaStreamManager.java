// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.media;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.webkit.ValueCallback;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.webrtc.MediaCaptureNotificationUtil;
import org.chromium.components.webrtc.MediaCaptureNotificationUtil.MediaType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.weblayer_private.IntentUtils;
import org.chromium.weblayer_private.TabImpl;
import org.chromium.weblayer_private.WebLayerImpl;
import org.chromium.weblayer_private.WebLayerNotificationChannels;
import org.chromium.weblayer_private.WebLayerNotificationWrapperBuilder;
import org.chromium.weblayer_private.interfaces.IMediaCaptureCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.HashSet;
import java.util.Set;

/**
 * A per-tab object that manages notifications for ongoing media capture streams
 * (microphone/camera). This object is created by {@link TabImpl} and creates and destroys its
 * native equivalent.
 */
@JNINamespace("weblayer")
public class MediaStreamManager {
    private static final String WEBRTC_PREFIX = "org.chromium.weblayer.webrtc";
    private static final String AV_STREAM_TAG = WEBRTC_PREFIX + ".avstream";

    /**
     * A key used in the app's shared preferences to track a set of active streaming notifications.
     * This is used to clear notifications that may have persisted across restarts due to a crash.
     * TODO(estade): remove this approach and simply iterate across all notifications via
     * {@link NotificationManager#getActiveNotifications} once the minimum API level is 23.
     */
    private static final String PREF_ACTIVE_AV_STREAM_NOTIFICATION_IDS =
            WEBRTC_PREFIX + ".avstream_notifications";

    private IMediaCaptureCallbackClient mClient;

    private TabImpl mTab;

    // The notification ID matches the tab ID, which uniquely identifies the notification when
    // paired with the tag.
    private int mNotificationId;

    // Pointer to the native MediaStreamManager.
    private long mNative;

    /**
     * To be called when WebLayer is started. Clears notifications that may have persisted from
     * before a crash.
     */
    public static void onWebLayerInit() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> staleNotificationIds =
                prefs.getStringSet(PREF_ACTIVE_AV_STREAM_NOTIFICATION_IDS, null);
        if (staleNotificationIds == null) return;

        NotificationManagerProxy manager = getNotificationManager();
        if (manager == null) return;

        for (String id : staleNotificationIds) {
            manager.cancel(AV_STREAM_TAG, Integer.parseInt(id));
        }
        prefs.edit().remove(PREF_ACTIVE_AV_STREAM_NOTIFICATION_IDS).apply();
    }

    public MediaStreamManager(TabImpl tab) {
        mTab = tab;
        mNotificationId = tab.getId();
        mNative = MediaStreamManagerJni.get().create(this, tab.getWebContents());
    }

    public void destroy() {
        cancelNotification();
        MediaStreamManagerJni.get().destroy(mNative);
        mNative = 0;
        mClient = null;
    }

    public void setClient(IMediaCaptureCallbackClient client) {
        mClient = client;
    }

    public void stopStreaming() {
        MediaStreamManagerJni.get().stopStreaming(mNative);
    }

    private void cancelNotification() {
        NotificationManagerProxy notificationManager = getNotificationManager();
        if (notificationManager != null) {
            notificationManager.cancel(AV_STREAM_TAG, mNotificationId);
        }
        notifyClient(false, false);
        updateActiveNotifications(false);
    }

    private void notifyClient(boolean audio, boolean video) {
        if (mClient != null) {
            try {
                mClient.onMediaCaptureStateChanged(audio, video);
            } catch (RemoteException e) {
                throw new AndroidRuntimeException(e);
            }
        }
    }

    /**
     * Updates the list of active notifications stored in the SharedPrefences.
     *
     * @param active if true, then {@link mNotificationId} will be added to the list of active
     *         notifications, otherwise it will be removed.
     */
    private void updateActiveNotifications(boolean active) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> activeIds = new HashSet<String>(
                prefs.getStringSet(PREF_ACTIVE_AV_STREAM_NOTIFICATION_IDS, new HashSet<String>()));
        if (active) {
            activeIds.add(Integer.toString(mNotificationId));
        } else {
            activeIds.remove(Integer.toString(mNotificationId));
        }
        prefs.edit()
                .putStringSet(PREF_ACTIVE_AV_STREAM_NOTIFICATION_IDS,
                        activeIds.isEmpty() ? null : activeIds)
                .apply();
    }

    @CalledByNative
    private void prepareToStream(boolean audio, boolean video, int requestId)
            throws RemoteException {
        if (mClient == null) {
            respondToStreamRequest(requestId, true);
        } else {
            mClient.onMediaCaptureRequested(
                    audio, video, ObjectWrapper.wrap(new ValueCallback<Boolean>() {
                        @Override
                        public void onReceiveValue(Boolean allowed) {
                            ThreadUtils.assertOnUiThread();
                            respondToStreamRequest(requestId, allowed.booleanValue());
                        }
                    }));
        }
    }

    private void respondToStreamRequest(int requestId, boolean allow) {
        if (mNative == 0) return;
        MediaStreamManagerJni.get().onClientReadyToStream(mNative, requestId, allow);
    }

    /**
     * Called after the tab's media streaming state has changed.
     *
     * A notification should be shown (or updated) iff one of the parameters is true, otherwise
     * any existing notification will be removed.
     *
     * @param audio true if the tab is streaming audio.
     * @param video true if the tab is streaming video.
     */
    @CalledByNative
    private void update(boolean audio, boolean video) {
        if (!audio && !video) {
            cancelNotification();
            return;
        }

        Context appContext = ContextUtils.getApplicationContext();
        Intent intent = IntentUtils.createBringTabToFrontIntent(mNotificationId);
        PendingIntentProvider contentIntent =
                PendingIntentProvider.getBroadcast(appContext, mNotificationId, intent, 0);

        int mediaType = audio && video ? MediaType.AUDIO_AND_VIDEO
                                       : audio ? MediaType.AUDIO_ONLY : MediaType.VIDEO_ONLY;

        NotificationWrapper notification = MediaCaptureNotificationUtil.createNotification(
                WebLayerNotificationWrapperBuilder.create(
                        WebLayerNotificationChannels.ChannelId.WEBRTC_CAM_AND_MIC,
                        new NotificationMetadata(0, AV_STREAM_TAG, mNotificationId)),
                mediaType,
                mTab.getProfile().isIncognito() ? null
                                                : mTab.getWebContents().getVisibleUrl().getSpec(),
                WebLayerImpl.getClientApplicationName(), contentIntent, null /*stopIntent*/);
        getNotificationManager().notify(notification);

        updateActiveNotifications(true);
        notifyClient(audio, video);
    }

    private static NotificationManagerProxy getNotificationManager() {
        return new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
    }

    @NativeMethods
    interface Natives {
        long create(MediaStreamManager caller, WebContents webContents);
        void destroy(long manager);
        void onClientReadyToStream(long nativeMediaStreamManager, int requestId, boolean allow);
        void stopStreaming(long nativeMediaStreamManager);
    }
}
