// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.NotificationManager;
import android.content.SharedPreferences;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.annotation.StringDef;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelDefinitions;
import org.chromium.components.browser_ui.notifications.channels.ChannelDefinitions.PredefinedChannel;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Defines notification channels for WebLayer. */
@RequiresApi(Build.VERSION_CODES.O)
public class WebLayerNotificationChannels extends ChannelDefinitions {
    /**
     * Version number identifying the current set of channels. This must be incremented whenever the
     * set of channels returned by {@link #getStartupChannelIds()} or {@link #getLegacyChannelIds()}
     * changes.
     */
    static final int sChannelsVersion = 0;
    static final String sChannelsVersionKey = "org.chromium.weblayer.notification_channels_version";

    private static class LazyHolder {
        private static WebLayerNotificationChannels sInstance = new WebLayerNotificationChannels();
    }

    public static WebLayerNotificationChannels getInstance() {
        return LazyHolder.sInstance;
    }

    private WebLayerNotificationChannels() {}

    /**
     * To define a new channel, add the channel ID to this StringDef and add a new entry to
     * PredefinedChannels.MAP below with the appropriate channel parameters. To remove an existing
     * channel, remove the ID from this StringDef, remove its entry from Predefined Channels.MAP,
     * and add it to the return value of {@link #getLegacyChannelIds()}.
     */
    @StringDef({ChannelId.ACTIVE_DOWNLOADS, ChannelId.COMPLETED_DOWNLOADS, ChannelId.MEDIA_PLAYBACK,
            ChannelId.WEBRTC_CAM_AND_MIC})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChannelId {
        String ACTIVE_DOWNLOADS = "org.chromium.weblayer.active_downloads";
        String COMPLETED_DOWNLOADS = "org.chromium.weblayer.completed_downloads";
        String MEDIA_PLAYBACK = "org.chromium.weblayer.media_playback";
        String WEBRTC_CAM_AND_MIC = "org.chromium.weblayer.webrtc_cam_and_mic";
    }

    @StringDef({ChannelGroupId.WEBLAYER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChannelGroupId {
        String WEBLAYER = "org.chromium.weblayer";
    }

    // Map defined in static inner class so it's only initialized lazily.
    @RequiresApi(Build.VERSION_CODES.N) // for NotificationManager.IMPORTANCE_* constants
    private static class PredefinedChannels {
        static final Map<String, PredefinedChannel> MAP;

        static {
            Map<String, PredefinedChannel> map = new HashMap<>();
            map.put(ChannelId.ACTIVE_DOWNLOADS,
                    PredefinedChannel.create(ChannelId.ACTIVE_DOWNLOADS,
                            R.string.notification_category_downloads,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.WEBLAYER));
            map.put(ChannelId.COMPLETED_DOWNLOADS,
                    PredefinedChannel.create(ChannelId.COMPLETED_DOWNLOADS,
                            R.string.notification_category_completed_downloads,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.WEBLAYER));
            map.put(ChannelId.MEDIA_PLAYBACK,
                    PredefinedChannel.create(ChannelId.MEDIA_PLAYBACK,
                            R.string.notification_category_media_playback,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.WEBLAYER));
            map.put(ChannelId.WEBRTC_CAM_AND_MIC,
                    PredefinedChannel.create(ChannelId.WEBRTC_CAM_AND_MIC,
                            R.string.notification_category_webrtc_cam_and_mic,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.WEBLAYER));
            MAP = Collections.unmodifiableMap(map);
        }
    }

    // Map defined in static inner class so it's only initialized lazily.
    private static class PredefinedChannelGroups {
        static final Map<String, PredefinedChannelGroup> MAP;
        static {
            Map<String, PredefinedChannelGroup> map = new HashMap<>();
            map.put(ChannelGroupId.WEBLAYER,
                    new PredefinedChannelGroup(ChannelGroupId.WEBLAYER,
                            R.string.weblayer_notification_channel_group_name));
            MAP = Collections.unmodifiableMap(map);
        }
    }

    @Override
    public Set<String> getAllChannelGroupIds() {
        return PredefinedChannelGroups.MAP.keySet();
    }

    @Override
    public Set<String> getAllChannelIds() {
        return PredefinedChannels.MAP.keySet();
    }

    @Override
    public Set<String> getStartupChannelIds() {
        return Collections.emptySet();
    }

    @Override
    public List<String> getLegacyChannelIds() {
        return Collections.emptyList();
    }

    @Override
    public PredefinedChannelGroup getChannelGroup(@ChannelGroupId String groupId) {
        return PredefinedChannelGroups.MAP.get(groupId);
    }

    @Override
    public PredefinedChannel getChannelFromId(@ChannelId String channelId) {
        return PredefinedChannels.MAP.get(channelId);
    }

    /**
     * Updates the user-facing channel names after a locale switch.
     */
    public static void onLocaleChanged() {
        if (!isAtLeastO()) return;

        getChannelsInitializer().updateLocale(ContextUtils.getApplicationContext().getResources());
    }

    /**
     * Updates the registered channels based on {@link sChannelsVersion}.
     */
    public static void updateChannelsIfNecessary() {
        if (!isAtLeastO()) return;

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        if (prefs.getInt(sChannelsVersionKey, -1) == sChannelsVersion) return;

        ChannelsInitializer initializer = getChannelsInitializer();
        initializer.deleteLegacyChannels();
        initializer.initializeStartupChannels();
        prefs.edit().putInt(sChannelsVersionKey, sChannelsVersion).apply();
    }

    private static boolean isAtLeastO() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
    }

    private static ChannelsInitializer getChannelsInitializer() {
        assert isAtLeastO();

        return new ChannelsInitializer(
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext()),
                getInstance(), ContextUtils.getApplicationContext().getResources());
    }
}
