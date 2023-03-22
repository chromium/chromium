// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.graphics.Bitmap;
import android.os.RemoteException;

import androidx.annotation.BinderThread;
import androidx.annotation.MainThread;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.webapk_install.IOnFinishInstallCallback;
import org.chromium.components.webapk_install.IWebApkInstallCoordinatorService;
import org.chromium.components.webapps.WebApkInstallResult;

/**
 * Contains functionality to connect to the {@link WebApkInstallCoordinatorService} in Chrome using
 * the {@link WebApkServiceConnection}, schedule the install of one WebAPK and receive the install
 * result via the {@link IOnFinishInstallCallback}.
 */
public class WebApkInstallSchedulerClient {
    private static final String TAG = "WebApkInstallClient";

    /**
     * Casts an int to {@link WebApkInstallResult}.
     */
    public @WebApkInstallResult static int asWebApkInstallResult(int webApkInstallResult) {
        return webApkInstallResult;
    }

    @MainThread
    Promise<Integer> startInstallTask(byte[] apkProto, Bitmap primaryIcon,
            boolean isPrimaryIconMaskable, IWebApkInstallCoordinatorService serviceInterface) {
        Promise<Integer> whenInstallTaskCompleted = new Promise<>();
        IOnFinishInstallCallback.Stub serviceCallback = new IOnFinishInstallCallback.Stub() {
            @Override
            @BinderThread
            public void handleOnFinishInstall(int result) {
                // Post the task back to the main thread as promises have to be accessed from a
                // single thread.
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT, () -> { whenInstallTaskCompleted.fulfill(result); });
            }
        };

        try {
            serviceInterface.scheduleInstallAsync(
                    apkProto, primaryIcon, isPrimaryIconMaskable, serviceCallback);
        } catch (RemoteException e) {
            Log.w(TAG, "Failed to schedule install with Chrome WebAPK install service.", e);
            whenInstallTaskCompleted.reject();
        }

        return whenInstallTaskCompleted;
    }

    /**
     * Schedules the install of one WebAPK with Chrome's {@link WebApkInstallCoordinatorService}.
     * The {@code onInstallFinishedCallback} is triggered when the install finished or failed.
     */
    @MainThread
    public static void scheduleInstall(byte[] apkProto, Bitmap primaryIcon,
            boolean isPrimaryIconMaskable, Callback<Integer> onInstallFinishedCallback) {
        WebApkInstallSchedulerClient client = new WebApkInstallSchedulerClient();

        WebApkServiceConnection webApkServiceConnection = new WebApkServiceConnection();
        Promise<IWebApkInstallCoordinatorService> whenServiceConnected =
                webApkServiceConnection.connect();
        whenServiceConnected
                .then(
                        /*fulfilled */
                        (IWebApkInstallCoordinatorService serviceInterface)
                                -> client.startInstallTask(apkProto, primaryIcon,
                                        isPrimaryIconMaskable, serviceInterface))
                .then(
                        /*fulfilled */
                        installResult
                        -> {
                            webApkServiceConnection.unbindIfNeeded();
                            onInstallFinishedCallback.onResult(
                                    asWebApkInstallResult(installResult));
                        },
                        /*rejected*/
                        err -> {
                            webApkServiceConnection.unbindIfNeeded();
                            onInstallFinishedCallback.onResult(WebApkInstallResult.FAILURE);
                        });
    }

    /**
     * Returns if the {@link WebApkInstallCoordinatorService} is available.
     */
    public static boolean isInstallServiceAvailable() {
        return WebApkServiceConnection.isInstallServiceAvailable();
    }
}
