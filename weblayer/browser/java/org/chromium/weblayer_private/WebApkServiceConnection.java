// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.ResolveInfo;
import android.os.IBinder;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.components.webapk_install.IWebApkInstallCoordinatorService;

/**
 * Service Connection to the {@link WebApkInstallCoordinatorService} in Chrome.
 */
class WebApkServiceConnection implements ServiceConnection {
    private static final String TAG = "WebApkServiceConn";

    private static final String BIND_WEBAPK_SCHEDULE_INSTALL_INTENT_ACTION =
            "org.chromium.intent.action.INSTALL_WEB_APK";

    // TODO(swestphal): set package name of production chrome apk
    private static final String CHROME_PACKAGE_NAME = "com.google.android.apps.chrome";

    private Context mContext;
    private boolean mIsBound;
    // The promise is fulfilled as soon as the connection is established, it will then provide the
    // {@link IWebApkInstallCoordinatorService}. It will be rejected if the connection to the
    // service cannot be established. Retrieve the {@code mPromiseServiceInterface} by calling
    // {@link connect()}.
    Promise<IWebApkInstallCoordinatorService> mPromiseServiceInterface;

    WebApkServiceConnection() {
        mContext = ContextUtils.getApplicationContext();
        mPromiseServiceInterface = new Promise<>();
    }

    private static Intent createChromeInstallServiceIntent() {
        Intent intent = new Intent(BIND_WEBAPK_SCHEDULE_INSTALL_INTENT_ACTION);
        intent.setPackage(CHROME_PACKAGE_NAME);
        return intent;
    }

    /**
     * Tries to bind to the service, returns a promise which will be fulfilled as
     * soon as the connection is established or rejected in the failure case.
     * This function must only be called once.
     */
    Promise<IWebApkInstallCoordinatorService> connect() {
        Intent intent = createChromeInstallServiceIntent();

        try {
            mIsBound = mContext.bindService(intent, this, Context.BIND_AUTO_CREATE);
            if (!mIsBound) {
                Log.w(TAG, "Failed to bind to Chrome install service.");
                mPromiseServiceInterface.reject();
            }
        } catch (SecurityException e) {
            Log.w(TAG, "SecurityException while binding to Chrome install service.", e);
            mPromiseServiceInterface.reject();
        }

        return mPromiseServiceInterface;
    }

    /**
     * Unbinds the service connection if it is currently bound.
     */
    void unbindIfNeeded() {
        if (mIsBound) {
            mContext.unbindService(this);
        }
        mIsBound = false;
    }

    /**
     * Returns if the {@link WebApkInstallCoordinatorService} is available.
     */
    public static boolean isInstallServiceAvailable() {
        ResolveInfo info = ContextUtils.getApplicationContext().getPackageManager().resolveService(
                createChromeInstallServiceIntent(), 0);
        return info != null;
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        IWebApkInstallCoordinatorService serviceInterface =
                IWebApkInstallCoordinatorService.Stub.asInterface(service);

        mPromiseServiceInterface.fulfill(serviceInterface);
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        if (!mPromiseServiceInterface.isFulfilled() && !mPromiseServiceInterface.isRejected()) {
            mPromiseServiceInterface.reject();
        }
        // Called when the Service closes the connection so we still might need to unbind.
        unbindIfNeeded();
    }
}
