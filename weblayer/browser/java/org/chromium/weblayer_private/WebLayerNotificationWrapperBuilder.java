// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.drawable.Icon;
import android.webkit.WebViewFactory;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.NotificationWrapperStandardBuilder;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/** A notification builder for WebLayer which has extra logic to make icons work correctly. */
public final class WebLayerNotificationWrapperBuilder extends NotificationWrapperStandardBuilder {
    /** Creates a notification builder. */
    public static WebLayerNotificationWrapperBuilder create(
            @WebLayerNotificationChannels.ChannelId String channelId,
            @NonNull NotificationMetadata metadata) {
        Context appContext = ContextUtils.getApplicationContext();
        ChannelsInitializer initializer =
                new ChannelsInitializer(new NotificationManagerProxyImpl(appContext),
                        WebLayerNotificationChannels.getInstance(), appContext.getResources());
        return new WebLayerNotificationWrapperBuilder(appContext, channelId, initializer, metadata);
    }

    private WebLayerNotificationWrapperBuilder(Context context, String channelId,
            ChannelsInitializer channelsInitializer, NotificationMetadata metadata) {
        super(context, channelId, channelsInitializer, metadata);
    }

    @Override
    public NotificationWrapperBuilder setSmallIcon(int icon) {
        if (WebLayerImpl.isAndroidResource(icon)) {
            super.setSmallIcon(icon);
        } else {
            super.setSmallIcon(createIcon(icon));
        }
        return this;
    }

    @Override
    @SuppressWarnings("deprecation")
    public NotificationWrapperBuilder addAction(
            int icon, CharSequence title, PendingIntent intent) {
        if (WebLayerImpl.isAndroidResource(icon)) {
            super.addAction(icon, title, intent);
        } else {
            super.addAction(
                    new Notification.Action.Builder(createIcon(icon), title, intent).build());
        }
        return this;
    }

    private Icon createIcon(int resId) {
        return Icon.createWithResource(WebViewFactory.getLoadedPackageInfo().packageName,
                WebLayerImpl.getResourceIdForSystemUi(resId));
    }

    /**
     * Finds a reasonable replacement for the given app-defined resource from among stock android
     * resources. This is useful when {@link Icon} is not available.
     */
    private int getFallbackAndroidResource(int appResourceId) {
        if (appResourceId == R.drawable.ic_play_arrow_white_24dp) {
            return android.R.drawable.ic_media_play;
        }
        if (appResourceId == R.drawable.ic_pause_white_24dp) {
            return android.R.drawable.ic_media_pause;
        }
        if (appResourceId == R.drawable.ic_stop_white_24dp) {
            // There's no ic_media_stop. This standin is at least a square. In practice this
            // shouldn't ever come up as stop is only used in (Chrome) cast notifications.
            return android.R.drawable.checkbox_off_background;
        }
        if (appResourceId == R.drawable.ic_skip_previous_white_24dp) {
            return android.R.drawable.ic_media_previous;
        }
        if (appResourceId == R.drawable.ic_skip_next_white_24dp) {
            return android.R.drawable.ic_media_next;
        }
        if (appResourceId == R.drawable.ic_fast_forward_white_24dp) {
            return android.R.drawable.ic_media_ff;
        }
        if (appResourceId == R.drawable.ic_fast_rewind_white_24dp) {
            return android.R.drawable.ic_media_rew;
        }
        if (appResourceId == R.drawable.audio_playing) {
            return android.R.drawable.ic_lock_silent_mode_off;
        }

        return android.R.drawable.radiobutton_on_background;
    }
}
