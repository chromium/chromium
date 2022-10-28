// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.media;

import android.app.Application;
import android.app.Service;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.support.v4.media.session.MediaSessionCompat;

import androidx.fragment.app.FragmentManager;
import androidx.mediarouter.media.MediaRouter;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.content_public.browser.WebContents;
import org.chromium.weblayer_private.IntentUtils;
import org.chromium.weblayer_private.TabImpl;
import org.chromium.weblayer_private.WebLayerFactoryImpl;
import org.chromium.weblayer_private.WebLayerImpl;
import org.chromium.weblayer_private.interfaces.RemoteMediaServiceConstants;

/** Provides WebLayer-specific behavior for Media Router. */
@JNINamespace("weblayer")
public class MediaRouterClientImpl extends MediaRouterClient {
    static int sPresentationNotificationId;
    static int sRemotingNotificationId;

    private MediaRouterClientImpl() {}

    public static void serviceStarted(Service service, Intent intent) {
        int notificationId = intent.getIntExtra(RemoteMediaServiceConstants.NOTIFICATION_ID_KEY, 0);
        if (notificationId == 0) {
            throw new RuntimeException("Invalid RemoteMediaService notification id");
        }
        MediaSessionNotificationHelper.serviceStarted(service, intent, notificationId);
    }

    public static void serviceDestroyed(int notificationId) {
        MediaSessionNotificationHelper.serviceDestroyed(notificationId);
    }

    @Override
    public Context getContextForRemoting() {
        return getContextForRemotingImpl();
    }

    @Override
    public int getTabId(WebContents webContents) {
        TabImpl tab = TabImpl.fromWebContents(webContents);
        return tab == null ? -1 : tab.getId();
    }

    @Override
    public Intent createBringTabToFrontIntent(int tabId) {
        return IntentUtils.createBringTabToFrontIntent(tabId);
    }

    @Override
    public void showNotification(MediaNotificationInfo notificationInfo) {
        MediaNotificationManager.show(notificationInfo, () -> {
            return new MediaRouterNotificationControllerDelegate(notificationInfo.id);
        });
    }

    @Override
    public int getPresentationNotificationId() {
        return getPresentationNotificationIdFromClient();
    }

    @Override
    public int getRemotingNotificationId() {
        return getRemotingNotificationIdFromClient();
    }

    @Override
    public FragmentManager getSupportFragmentManager(WebContents initiator) {
        return TabImpl.fromWebContents(initiator)
                .getBrowser()
                .createMediaRouteDialogFragment()
                .getSupportFragmentManager();
    }

    @Override
    // TODO(crbug.com/1377518): Implement addDeferredTask().
    public void addDeferredTask(Runnable deferredTask) {
        deferredTask.run();
    }

    @Override
    public boolean isCafMrpDeferredDiscoveryEnabled() {
        return true;
    }

    @CalledByNative
    public static void initialize() {
        if (MediaRouterClient.getInstance() != null) return;

        MediaRouterClient.setInstance(new MediaRouterClientImpl());
    }

    @CalledByNative
    public static boolean isMediaRouterEnabled() {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 88) return false;

        Context context = ContextUtils.getApplicationContext();
        try {
            ApplicationInfo ai = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            return ai.metaData.getBoolean(RemoteMediaServiceConstants.FEATURE_ENABLED_KEY, true);
        } catch (NameNotFoundException e) {
            return true;
        }
    }

    private static class MediaRouterNotificationControllerDelegate
            implements MediaNotificationController.Delegate {
        // The ID distinguishes between Presentation and Remoting services/notifications.
        private final int mNotificationId;

        MediaRouterNotificationControllerDelegate(int notificationId) {
            mNotificationId = notificationId;
        }

        @Override
        public Intent createServiceIntent() {
            return WebLayerImpl.createRemoteMediaServiceIntent().putExtra(
                    RemoteMediaServiceConstants.NOTIFICATION_ID_KEY, mNotificationId);
        }

        @Override
        public String getAppName() {
            return WebLayerImpl.getClientApplicationName();
        }

        @Override
        public String getNotificationGroupName() {
            if (mNotificationId == getPresentationNotificationIdFromClient()) {
                return "org.chromium.weblayer.PresentationApi";
            }

            assert mNotificationId == getRemotingNotificationIdFromClient();
            return "org.chromium.weblayer.RemotePlaybackApi";
        }

        @Override
        public NotificationWrapperBuilder createNotificationWrapperBuilder() {
            return MediaSessionNotificationHelper.createNotificationWrapperBuilder(mNotificationId);
        }

        @Override
        public void onMediaSessionUpdated(MediaSessionCompat session) {
            MediaRouter.getInstance(getContextForRemotingImpl()).setMediaSessionCompat(session);
        }

        @Override
        public void logNotificationShown(NotificationWrapper notification) {}
    }

    private static int getPresentationNotificationIdFromClient() {
        if (sPresentationNotificationId == 0) {
            sPresentationNotificationId = WebLayerImpl.getPresentationApiNotificationId();
        }
        return sPresentationNotificationId;
    }

    private static int getRemotingNotificationIdFromClient() {
        if (sRemotingNotificationId == 0) {
            sRemotingNotificationId = WebLayerImpl.getRemotePlaybackApiNotificationId();
        }
        return sRemotingNotificationId;
    }

    private static Context getContextForRemotingImpl() {
        Context context = ContextUtils.getApplicationContext();
        // The GMS Cast framework assumes the passed {@link Context} returns an instance of {@link
        // Application} from {@link getApplicationContext()}, so we make sure to remove any
        // wrappers.
        while (!(context.getApplicationContext() instanceof Application)) {
            if (context instanceof ContextWrapper) {
                context = ((ContextWrapper) context).getBaseContext();
            } else {
                return null;
            }
        }
        return context;
    }
}
