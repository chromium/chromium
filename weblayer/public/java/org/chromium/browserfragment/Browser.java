// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.interfaces.IBrowserSandboxCallback;
import org.chromium.browserfragment.interfaces.IBrowserSandboxService;

import java.util.ArrayList;
import java.util.List;

/**
 * Handle to the Browsing Sandbox. Must be created asynchronously.
 */
public class Browser {
    // TODO(swestphal): Remove this and its function args and detect which service should be used
    // based on android version.
    private static final String BROWSER_PROCESS_MODE =
            "org.chromium.browserfragment.shell.BrowserProcessMode";

    // Use another APK as a placeholder for an actual sandbox, since they are conceptually the
    // same thing.
    private static final String SANDBOX_BROWSER_SANDBOX_PACKAGE =
            "org.chromium.browserfragment.sandbox";

    private static final String BROWSER_SANDBOX_ACTION =
            "org.chromium.weblayer.intent.action.BROWSERSANDBOX";
    private static final String BROWSER_INPROCESS_ACTION =
            "org.chromium.weblayer.intent.action.BROWSERINPROCESS";

    private static final String DEFAULT_PROFILE_NAME = "DefaultProfile";

    private static Browser sInstance;
    private static boolean sPendingConnection;

    private ConnectionSetup mConnection;
    private IBrowserSandboxService mBrowserSandboxService;

    private List<BrowserFragment> mActiveFragments = new ArrayList<>();

    private static class ConnectionSetup implements ServiceConnection {
        private CallbackToFutureAdapter.Completer<Browser> mCompleter;
        private IBrowserSandboxService mBrowserSandboxService;
        private Context mContext;

        private final IBrowserSandboxCallback mBrowserSandboxCallback =
                new IBrowserSandboxCallback.Stub() {
                    @Override
                    public void onBrowserProcessInitialized() {
                        sInstance = new Browser(ConnectionSetup.this, mBrowserSandboxService);
                        sPendingConnection = false;
                        mCompleter.set(sInstance);
                        mCompleter = null;
                    }
                };

        ConnectionSetup(Context context, CallbackToFutureAdapter.Completer<Browser> completer) {
            mContext = context;
            mCompleter = completer;
        }

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            mBrowserSandboxService = IBrowserSandboxService.Stub.asInterface(service);
            try {
                mBrowserSandboxService.initializeBrowserProcess(mBrowserSandboxCallback);
            } catch (RemoteException e) {
                mCompleter.setException(e);
                mCompleter = null;
            }
        }

        void unbind() {
            mContext.unbindService(this);
            sInstance = null;
        }

        // TODO(rayankans): Actually handle failure / disconnection events.
        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private Browser(ConnectionSetup connection, IBrowserSandboxService service) {
        mConnection = connection;
        mBrowserSandboxService = service;
    }

    /**
     * Asynchronously creates a handle to the browsing sandbox after initializing the
     * browser process.
     * @param context The application context.
     */
    @NonNull
    public static ListenableFuture<Browser> create(@NonNull Context context) {
        if (sInstance != null) {
            return Futures.immediateFuture(sInstance);
        }
        if (sPendingConnection) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("Browser is already being created"));
        }
        sPendingConnection = true;
        return CallbackToFutureAdapter.getFuture(completer -> {
            // Use the application context since the Browser Sandbox might out live the Activity.
            Context applicationContext = context.getApplicationContext();

            ConnectionSetup connectionSetup = new ConnectionSetup(applicationContext, completer);
            Intent intent =
                    new Intent(isInProcessMode(applicationContext) ? BROWSER_INPROCESS_ACTION
                                                                   : BROWSER_SANDBOX_ACTION);
            intent.setPackage(isInProcessMode(applicationContext)
                            ? applicationContext.getPackageName()
                            : SANDBOX_BROWSER_SANDBOX_PACKAGE);

            applicationContext.bindService(intent, connectionSetup, Context.BIND_AUTO_CREATE);

            // Debug string.
            return "Browser Sandbox Future";
        });
    }

    /**
     * Creates a new BrowserFragment for displaying web content.
     */
    @Nullable
    public BrowserFragment createFragment() {
        FragmentParams params =
                (new FragmentParams.Builder()).setProfileName(DEFAULT_PROFILE_NAME).build();
        return createFragment(params);
    }

    /**
     * Creates a new BrowserFragment for displaying web content.
     */
    @Nullable
    public BrowserFragment createFragment(FragmentParams params) {
        if (mBrowserSandboxService == null) {
            throw new IllegalStateException("Browser has been destroyed");
        }
        try {
            BrowserFragment fragment = new BrowserFragment();
            fragment.initialize(
                    this, mBrowserSandboxService.createFragmentDelegate(params.getParcelable()));
            return fragment;
        } catch (RemoteException e) {
            return null;
        }
    }

    /**
     * Enables or disables DevTools remote debugging.
     */
    public void setRemoteDebuggingEnabled(boolean enabled) {
        if (mBrowserSandboxService == null) {
            throw new IllegalStateException("Browser has been destroyed");
        }
        try {
            mBrowserSandboxService.setRemoteDebuggingEnabled(enabled);
        } catch (RemoteException e) {
        }
    }

    // TODO(swestphal): Remove this again.
    static boolean isInProcessMode(Context appContext) {
        try {
            Bundle metaData = appContext.getPackageManager()
                                      .getApplicationInfo(appContext.getPackageName(),
                                              PackageManager.GET_META_DATA)
                                      .metaData;
            if (metaData != null) return metaData.getString(BROWSER_PROCESS_MODE).equals("local");
        } catch (PackageManager.NameNotFoundException e) {
        }
        return false;
    }

    void addFragment(BrowserFragment fragment) {
        mActiveFragments.add(fragment);
    }

    void removeFragment(BrowserFragment fragment) {
        mActiveFragments.remove(fragment);
    }

    boolean isShutdown() {
        return mBrowserSandboxService == null;
    }

    public void shutdown() {
        if (isShutdown()) {
            // Browser was already shut down.
            return;
        }

        for (BrowserFragment fragment : mActiveFragments) {
            // This will detach the fragment, save the state, and remove |fragment| from
            // |mActiveFragments|.
            fragment.invalidate();
        }

        mBrowserSandboxService = null;
        mConnection.unbind();
    }
}
