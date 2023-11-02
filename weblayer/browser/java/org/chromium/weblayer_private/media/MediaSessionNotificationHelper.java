// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.media;

import android.app.Service;
import android.content.Intent;

import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.weblayer_private.WebLayerNotificationChannels;
import org.chromium.weblayer_private.WebLayerNotificationWrapperBuilder;

/**
 * A helper class for management of MediaSession (local device), Presentation API and Remote
 * Playback API (casting) notifications and foreground services.
 */
class MediaSessionNotificationHelper {
    static void serviceStarted(Service service, Intent intent, int notificationId) {
        MediaNotificationController controller =
                MediaNotificationManager.getController(notificationId);
        if (controller != null && controller.processIntent(service, intent)) return;

        // The service has been started with startForegroundService() but the
        // notification hasn't been shown. See similar logic in {@link
        // ChromeMediaNotificationControllerDelegate}.
        MediaNotificationController.finishStartingForegroundServiceOnO(service,
                createNotificationWrapperBuilder(notificationId).buildNotificationWrapper());
        // Call stopForeground to guarantee Android unset the foreground bit.
        ForegroundServiceUtils.getInstance().stopForeground(
                service, Service.STOP_FOREGROUND_REMOVE);
        service.stopSelf();
    }

    static void serviceDestroyed(int notificationId) {
        MediaNotificationController controller =
                MediaNotificationManager.getController(notificationId);
        if (controller != null) controller.onServiceDestroyed();
        MediaNotificationManager.clear(notificationId);
    }

    static NotificationWrapperBuilder createNotificationWrapperBuilder(int notificationId) {
        // Only the null tag will work as expected, because {@link Service#startForeground()} only
        // takes an ID and no tag. If we pass a tag here, then the notification that's used to
        // display a paused state (no foreground service) will not be identified as the same one
        // that's used with the foreground service.
        return WebLayerNotificationWrapperBuilder.create(
                WebLayerNotificationChannels.ChannelId.MEDIA_PLAYBACK,
                new NotificationMetadata(0, null /*notificationTag*/, notificationId));
    }
}
