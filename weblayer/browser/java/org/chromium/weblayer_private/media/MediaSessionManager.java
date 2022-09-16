// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.media;

import android.app.Service;
import android.content.Intent;
import android.support.v4.media.session.MediaSessionCompat;

import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.weblayer_private.IntentUtils;
import org.chromium.weblayer_private.TabImpl;
import org.chromium.weblayer_private.WebLayerImpl;

/**
 * A glue class for MediaSession.
 * This class defines delegates that provide WebLayer-specific behavior to shared MediaSession code.
 * It also manages the lifetime of {@link MediaNotificationController} and the {@link Service}
 * associated with the notification.
 */
public class MediaSessionManager {
    private static int sNotificationId;

    public static void serviceStarted(Service service, Intent intent) {
        MediaSessionNotificationHelper.serviceStarted(service, intent, getNotificationId());
    }

    public static void serviceDestroyed() {
        MediaSessionNotificationHelper.serviceDestroyed(getNotificationId());
    }

    public static MediaSessionHelper.Delegate createMediaSessionHelperDelegate(TabImpl tab) {
        return new MediaSessionHelper.Delegate() {
            @Override
            public Intent createBringTabToFrontIntent() {
                return IntentUtils.createBringTabToFrontIntent(tab.getId());
            }

            @Override
            public BrowserContextHandle getBrowserContextHandle() {
                return tab.getProfile();
            }

            @Override
            public MediaNotificationInfo.Builder createMediaNotificationInfoBuilder() {
                return new MediaNotificationInfo.Builder()
                        .setInstanceId(tab.getId())
                        .setId(getNotificationId());
            }

            @Override
            public void showMediaNotification(MediaNotificationInfo notificationInfo) {
                assert notificationInfo.id == getNotificationId();
                MediaNotificationManager.show(notificationInfo,
                        () -> { return new WebLayerMediaNotificationControllerDelegate(); });
            }

            @Override
            public void hideMediaNotification() {
                MediaNotificationManager.hide(tab.getId(), getNotificationId());
            }

            @Override
            public void activateAndroidMediaSession() {
                MediaNotificationManager.activateAndroidMediaSession(
                        tab.getId(), getNotificationId());
            }
        };
    }

    private static class WebLayerMediaNotificationControllerDelegate
            implements MediaNotificationController.Delegate {
        @Override
        public Intent createServiceIntent() {
            return WebLayerImpl.createMediaSessionServiceIntent();
        }

        @Override
        public String getAppName() {
            return WebLayerImpl.getClientApplicationName();
        }

        @Override
        public String getNotificationGroupName() {
            return "org.chromium.weblayer.MediaSession";
        }

        @Override
        public NotificationWrapperBuilder createNotificationWrapperBuilder() {
            return MediaSessionNotificationHelper.createNotificationWrapperBuilder(
                    getNotificationId());
        }

        @Override
        public void onMediaSessionUpdated(MediaSessionCompat session) {
            // This is only relevant when casting.
        }

        @Override
        public void logNotificationShown(NotificationWrapper notification) {}
    }

    private static int getNotificationId() {
        if (sNotificationId == 0) sNotificationId = WebLayerImpl.getMediaSessionNotificationId();
        return sNotificationId;
    }
}
