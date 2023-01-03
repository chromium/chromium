// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.RemoteException;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.core.app.NotificationCompat;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.DownloadError;
import org.chromium.weblayer_private.interfaces.DownloadState;
import org.chromium.weblayer_private.interfaces.IClientDownload;
import org.chromium.weblayer_private.interfaces.IDownload;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.io.File;
import java.util.HashMap;

/**
 * Implementation of IDownload.
 */
@JNINamespace("weblayer")
public final class DownloadImpl extends IDownload.Stub {
    private static final String DOWNLOADS_PREFIX = "org.chromium.weblayer.downloads";

    // These actions have to be synchronized with the receiver defined in AndroidManifest.xml.
    private static final String OPEN_INTENT = DOWNLOADS_PREFIX + ".OPEN";
    private static final String ACTIVATE_TRANSIENT_INTENT =
            DOWNLOADS_PREFIX + ".ACTIVATE_TRANSIENT";
    private static final String DELETE_INTENT = DOWNLOADS_PREFIX + ".DELETE";
    private static final String PAUSE_INTENT = DOWNLOADS_PREFIX + ".PAUSE";
    private static final String RESUME_INTENT = DOWNLOADS_PREFIX + ".RESUME";
    private static final String CANCEL_INTENT = DOWNLOADS_PREFIX + ".CANCEL";

    private static final String EXTRA_NOTIFICATION_ID = DOWNLOADS_PREFIX + ".NOTIFICATION_ID";
    private static final String EXTRA_NOTIFICATION_LOCATION =
            DOWNLOADS_PREFIX + ".NOTIFICATION_LOCATION";
    private static final String EXTRA_NOTIFICATION_MIME_TYPE =
            DOWNLOADS_PREFIX + ".NOTIFICATION_MIME_TYPE";
    private static final String EXTRA_NOTIFICATION_PROFILE =
            DOWNLOADS_PREFIX + ".NOTIFICATION_PROFILE";
    private static final String EXTRA_NOTIFICATION_PROFILE_IS_INCOGNITO =
            DOWNLOADS_PREFIX + ".NOTIFICATION_PROFILE_IS_INCOGNITO";
    private static final String EXTRA_NOTIFICATION_SESSION_ID =
            DOWNLOADS_PREFIX + ".NOTIFICATION_SESSION_ID";
    // The intent prefix is used as the notification's tag since it's guaranteed not to conflict
    // with intent prefixes used by other subsystems that display notifications.
    private static final String NOTIFICATION_TAG = DOWNLOADS_PREFIX;
    private static final String TAG = "DownloadImpl";

    private final String mProfileName;
    private final boolean mIsIncognito;
    // The client is only used for downloads to disk, so it will be null for transient downloads.
    private final IDownloadCallbackClient mClient;
    private final IClientDownload mClientDownload;
    // WARNING: DownloadImpl may outlive the native side, in which case this member is set to 0.
    private long mNativeDownloadImpl;
    private boolean mDisableNotification;

    // The time this download started, in milliseconds.
    private final long mStartTime;

    // A transient download is not persisted to disk, which affects its UI treatment.
    private final boolean mIsTransient;

    // The originating URL for this download.
    private final GURL mSourceUrl;

    // The large icon to show. Once this is successfully fetched from native, it won't be updated.
    private Bitmap mLargeIcon;

    private final int mNotificationId;
    private static final HashMap<Integer, DownloadImpl> sMap = new HashMap<Integer, DownloadImpl>();

    /**
     * @return a string that prefixes all intents that can be handled by {@link forwardIntent}.
     */
    public static String getIntentPrefix() {
        return DOWNLOADS_PREFIX;
    }

    public static void forwardIntent(
            Context context, Intent intent, ProfileManager profileManager) {
        if (intent.getAction().equals(OPEN_INTENT)) {
            String location = intent.getStringExtra(EXTRA_NOTIFICATION_LOCATION);
            if (TextUtils.isEmpty(location)) {
                Log.d(TAG, "Didn't find location for open intent");
                return;
            }

            String mimeType = intent.getStringExtra(EXTRA_NOTIFICATION_MIME_TYPE);

            Intent openIntent = new Intent(Intent.ACTION_VIEW);
            if (TextUtils.isEmpty(mimeType)) {
                openIntent.setData(getDownloadUri(location));
            } else {
                openIntent.setDataAndType(getDownloadUri(location), mimeType);
            }
            openIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            openIntent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            openIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try {
                context.startActivity(openIntent);
            } catch (ActivityNotFoundException ex) {
                // TODO: show some UI that there were no apps to handle this?
            }

            return;
        }

        String profileName = intent.getStringExtra(EXTRA_NOTIFICATION_PROFILE);
        boolean isIncognito;
        if (intent.hasExtra(EXTRA_NOTIFICATION_PROFILE_IS_INCOGNITO)) {
            isIncognito = intent.getBooleanExtra(EXTRA_NOTIFICATION_PROFILE_IS_INCOGNITO, false);
        } else {
            isIncognito = "".equals(profileName);
        }

        ProfileImpl profile = profileManager.getProfile(profileName, isIncognito);
        if (!profile.areDownloadsInitialized()) {
            profile.addDownloadNotificationIntent(intent);
        } else {
            handleIntent(intent);
        }
    }

    public static void handleIntent(Intent intent) {
        int id = intent.getIntExtra(EXTRA_NOTIFICATION_ID, -1);
        DownloadImpl download = sMap.get(id);
        if (download == null) {
            Log.d(TAG, "Didn't find download for " + id);
            // TODO(jam): handle download resumption after restart
            return;
        }

        if (intent.getAction().equals(PAUSE_INTENT)) {
            download.pause();
        } else if (intent.getAction().equals(RESUME_INTENT)) {
            download.resume();
        } else if (intent.getAction().equals(CANCEL_INTENT)) {
            download.cancel();
        } else if (intent.getAction().equals(DELETE_INTENT)) {
            sMap.remove(id);
            DownloadImplJni.get().onFinishedImpl(download.mNativeDownloadImpl, /*activated=*/false);
        } else if (intent.getAction().equals(ACTIVATE_TRANSIENT_INTENT)) {
            assert download.mIsTransient;
            DownloadImplJni.get().onFinishedImpl(download.mNativeDownloadImpl, /*activated=*/true);
        }
    }

    public DownloadImpl(String profileName, boolean isIncognito, IDownloadCallbackClient client,
            long nativeDownloadImpl, int id, boolean isTransient, GURL sourceUrl) {
        mProfileName = profileName;
        mIsIncognito = isIncognito;
        mClient = isTransient ? null : client;
        mNativeDownloadImpl = nativeDownloadImpl;
        mNotificationId = id;
        mStartTime = System.currentTimeMillis();
        mIsTransient = isTransient;
        mSourceUrl = sourceUrl;

        if (mClient == null) {
            mClientDownload = null;
        } else {
            try {
                mClientDownload = mClient.createClientDownload(this);
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        DownloadImplJni.get().setJavaDownload(mNativeDownloadImpl, DownloadImpl.this);
    }

    public IClientDownload getClientDownload() {
        return mClientDownload;
    }

    @DownloadState
    private static int implStateToJavaType(@ImplDownloadState int type) {
        switch (type) {
            case ImplDownloadState.IN_PROGRESS:
                return DownloadState.IN_PROGRESS;
            case ImplDownloadState.COMPLETE:
                return DownloadState.COMPLETE;
            case ImplDownloadState.PAUSED:
                return DownloadState.PAUSED;
            case ImplDownloadState.CANCELLED:
                return DownloadState.CANCELLED;
            case ImplDownloadState.FAILED:
                return DownloadState.FAILED;
        }
        assert false;
        return DownloadState.FAILED;
    }

    @DownloadError
    private static int implErrorToJavaType(@ImplDownloadError int type) {
        switch (type) {
            case ImplDownloadError.NO_ERROR:
                return DownloadError.NO_ERROR;
            case ImplDownloadError.SERVER_ERROR:
                return DownloadError.SERVER_ERROR;
            case ImplDownloadError.SSL_ERROR:
                return DownloadError.SSL_ERROR;
            case ImplDownloadError.CONNECTIVITY_ERROR:
                return DownloadError.CONNECTIVITY_ERROR;
            case ImplDownloadError.NO_SPACE:
                return DownloadError.NO_SPACE;
            case ImplDownloadError.FILE_ERROR:
                return DownloadError.FILE_ERROR;
            case ImplDownloadError.CANCELLED:
                return DownloadError.CANCELLED;
            case ImplDownloadError.OTHER_ERROR:
                return DownloadError.OTHER_ERROR;
        }
        assert false;
        return DownloadError.OTHER_ERROR;
    }

    @Override
    @DownloadState
    public int getState() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implStateToJavaType(DownloadImplJni.get().getStateImpl(mNativeDownloadImpl));
    }

    @Override
    public long getTotalBytes() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getTotalBytesImpl(mNativeDownloadImpl);
    }

    @Override
    public long getReceivedBytes() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getReceivedBytesImpl(mNativeDownloadImpl);
    }

    @Override
    public void pause() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().pauseImpl(mNativeDownloadImpl);
        updateNotification();
    }

    @Override
    public void resume() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().resumeImpl(mNativeDownloadImpl);
        updateNotification();
    }

    @Override
    public void cancel() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().cancelImpl(mNativeDownloadImpl);
        updateNotification();
    }

    @Override
    public String getLocation() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getLocationImpl(mNativeDownloadImpl);
    }

    @Override
    public String getFileNameToReportToUser() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getFileNameToReportToUserImpl(mNativeDownloadImpl);
    }

    @Override
    public String getMimeType() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getMimeTypeImpl(mNativeDownloadImpl);
    }

    @Override
    @DownloadError
    public int getError() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implErrorToJavaType(DownloadImplJni.get().getErrorImpl(mNativeDownloadImpl));
    }

    @Override
    public void disableNotification() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        mDisableNotification = true;

        NotificationManagerProxy notificationManager = getNotificationManager();
        notificationManager.cancel(NOTIFICATION_TAG, mNotificationId);
    }

    private void throwIfNativeDestroyed() {
        if (mNativeDownloadImpl == 0) {
            throw new IllegalStateException("Using Download after native destroyed");
        }
    }

    private Intent createIntent(String actionId) {
        // Because the intent is using classes from the implementation's class loader,
        // we need to use the constructor which doesn't take the app's context.
        Intent intent = WebLayerImpl.createIntent();
        intent.setAction(actionId);
        intent.putExtra(EXTRA_NOTIFICATION_ID, mNotificationId);
        intent.putExtra(EXTRA_NOTIFICATION_PROFILE, mProfileName);
        intent.putExtra(EXTRA_NOTIFICATION_PROFILE_IS_INCOGNITO, mIsIncognito);
        return intent;
    }

    public void downloadStarted() {
        if (mDisableNotification) return;

        // TODO(jam): create a foreground service while the download is running to avoid the process
        // being shut down if the user switches apps.

        sMap.put(Integer.valueOf(mNotificationId), this);

        updateNotification();
    }

    public void downloadProgressChanged() {
        updateNotification();
    }

    public void downloadCompleted() {
        updateNotification();
    }

    public void downloadFailed() {
        updateNotification();
    }

    private void updateNotification() {
        NotificationManagerProxy notificationManager = getNotificationManager();
        if (mDisableNotification || notificationManager == null) return;

        Context context = ContextUtils.getApplicationContext();

        Intent deleteIntent = createIntent(DELETE_INTENT);
        PendingIntentProvider deletePendingIntent = getPendingIntentProvider(deleteIntent);

        @DownloadState
        int state = getState();
        if (state == DownloadState.CANCELLED) {
            notificationManager.cancel(NOTIFICATION_TAG, mNotificationId);
            mDisableNotification = true;
            return;
        }

        String channelId = state == DownloadState.COMPLETE
                ? WebLayerNotificationChannels.ChannelId.COMPLETED_DOWNLOADS
                : WebLayerNotificationChannels.ChannelId.ACTIVE_DOWNLOADS;

        WebLayerNotificationWrapperBuilder builder = WebLayerNotificationWrapperBuilder.create(
                channelId, new NotificationMetadata(0, NOTIFICATION_TAG, mNotificationId));
        builder.setOngoing(true)
                .setWhen(mStartTime)
                .setShowWhen(true)
                .setDeleteIntent(deletePendingIntent)
                .setPriorityBeforeO(NotificationCompat.PRIORITY_DEFAULT);

        // The filename might not have been available initially.
        String name = getFileNameToReportToUser();
        if (!TextUtils.isEmpty(name)) {
            builder.setContentTitle(name);
        }

        // Set the large icon/thumbnail, except when incognito.
        if (!mIsIncognito && mLargeIcon == null) {
            mLargeIcon = DownloadImplJni.get().getLargeIconImpl(mNativeDownloadImpl);
        }
        if (mLargeIcon != null) {
            builder.setLargeIcon(mLargeIcon);
        }

        // As with Chrome, transient downloads "promote" the source URL.
        if (!mIsIncognito && mIsTransient) {
            String formattedUrl = DownloadUtils.formatUrlForDisplayInNotification(mSourceUrl);
            if (formattedUrl != null) builder.setSubText(formattedUrl);
        }
        // TODO(estade): In incognito, Chrome uses a subtext of "Incognito tab". Should WL display
        // something similar?

        Resources resources = context.getResources();

        if (state == DownloadState.COMPLETE) {
            builder.setOngoing(false)
                    .setSmallIcon(android.R.drawable.stat_sys_download_done)
                    .setAutoCancel(true)
                    .setProgress(0, 0, false);

            Intent openIntent = null;

            if (mIsTransient) {
                builder.setContentText(
                        resources.getString(R.string.download_notification_completed));
                openIntent = createIntent(ACTIVATE_TRANSIENT_INTENT);
            } else {
                builder.setContentText(
                        resources.getString(R.string.download_notification_completed_with_size,
                                DownloadUtils.getStringForBytes(context, getTotalBytes())));

                openIntent = createIntent(OPEN_INTENT);
                openIntent.putExtra(EXTRA_NOTIFICATION_LOCATION, getLocation());
                openIntent.putExtra(EXTRA_NOTIFICATION_MIME_TYPE, getMimeType());
            }
            builder.setContentIntent(getPendingIntentProvider(openIntent));
        } else if (state == DownloadState.FAILED) {
            builder.setContentText(resources.getString(R.string.download_notification_failed))
                    .setOngoing(false)
                    .setSmallIcon(android.R.drawable.stat_sys_download_done)
                    .setProgress(0, 0, false);
        } else if (state == DownloadState.IN_PROGRESS) {
            Intent pauseIntent = createIntent(PAUSE_INTENT);
            PendingIntentProvider pausePendingIntent = getPendingIntentProvider(pauseIntent);

            long bytes = getReceivedBytes();
            long totalBytes = getTotalBytes();
            boolean indeterminate = totalBytes == -1;
            int progressCurrent = -1;
            if (!indeterminate && totalBytes != 0) {
                progressCurrent = (int) (bytes * 100 / totalBytes);
            }

            if (!mIsTransient) {
                String contentText;
                String bytesString = DownloadUtils.getStringForBytes(context, bytes);
                if (indeterminate) {
                    contentText = resources.getString(
                            R.string.download_ui_indeterminate_bytes, bytesString);
                } else {
                    String totalString = DownloadUtils.getStringForBytes(context, totalBytes);
                    contentText = resources.getString(
                            R.string.download_ui_determinate_bytes, bytesString, totalString);
                }
                builder.setContentText(contentText);
            }
            builder.addAction(0 /* no icon */,
                           resources.getString(R.string.download_notification_pause_button),
                           pausePendingIntent, 0 /* no action for UMA */)
                    .setSmallIcon(android.R.drawable.stat_sys_download)
                    .setProgress(100, progressCurrent, indeterminate);
        } else if (state == DownloadState.PAUSED) {
            Intent resumeIntent = createIntent(RESUME_INTENT);
            PendingIntentProvider resumePendingIntent = getPendingIntentProvider(resumeIntent);
            builder.setContentText(resources.getString(R.string.download_notification_paused))
                    .addAction(0 /* no icon */,
                            resources.getString(R.string.download_notification_resume_button),
                            resumePendingIntent, 0 /* no action for UMA */)
                    .setSmallIcon(android.R.drawable.ic_media_pause)
                    .setProgress(0, 0, false);
        }

        if (state == DownloadState.IN_PROGRESS || state == DownloadState.PAUSED) {
            Intent cancelIntent = createIntent(CANCEL_INTENT);
            PendingIntentProvider cancelPendingIntent = getPendingIntentProvider(cancelIntent);
            builder.addAction(0 /* no icon */,
                    resources.getString(R.string.download_notification_cancel_button),
                    cancelPendingIntent, 0 /* no action for UMA */);
        }

        notificationManager.notify(builder.buildNotificationWrapper());
    }

    private PendingIntentProvider getPendingIntentProvider(Intent notificationIntent) {
        // Transient intents use FLAG_CANCEL_CURRENT because the IDs can overlap across sessions.
        // CANCEL_CURRENT makes sure the PendingIntent is not also reused, and prevents intents from
        // old sessions from working (e.g. notifications lingering after WebLayer has crashed and
        // failed to clear them).
        return PendingIntentProvider.getBroadcast(ContextUtils.getApplicationContext(),
                mNotificationId, notificationIntent,
                mIsTransient ? PendingIntent.FLAG_CANCEL_CURRENT : 0);
    }

    /**
     * Returns the notification manager.
     */
    private static NotificationManagerProxy getNotificationManager() {
        return new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
    }

    private static Uri getDownloadUri(String location) {
        if (ContentUriUtils.isContentUri(location)) return Uri.parse(location);
        return ContentUriUtils.getContentUriFromFile(new File(location));
    }

    @VisibleForTesting
    public static void activateNotificationForTesting(int id) {
        DownloadImpl download = sMap.get(id);
        assert download != null;
        DownloadImplJni.get().onFinishedImpl(download.mNativeDownloadImpl, /*activated=*/true);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeDownloadImpl = 0;
        sMap.remove(mNotificationId);
        if (mIsTransient) {
            getNotificationManager().cancel(NOTIFICATION_TAG, mNotificationId);
        }
        // TODO: this should likely notify delegate in some way.
    }

    @NativeMethods
    interface Natives {
        void setJavaDownload(long nativeDownloadImpl, DownloadImpl caller);
        int getStateImpl(long nativeDownloadImpl);
        long getTotalBytesImpl(long nativeDownloadImpl);
        long getReceivedBytesImpl(long nativeDownloadImpl);
        void pauseImpl(long nativeDownloadImpl);
        void resumeImpl(long nativeDownloadImpl);
        void cancelImpl(long nativeDownloadImpl);
        void onFinishedImpl(long nativeDownloadImpl, boolean activated);
        String getLocationImpl(long nativeDownloadImpl);
        String getFileNameToReportToUserImpl(long nativeDownloadImpl);
        String getMimeTypeImpl(long nativeDownloadImpl);
        int getErrorImpl(long nativeDownloadImpl);
        Bitmap getLargeIconImpl(long nativeDownloadImpl);
    }
}
