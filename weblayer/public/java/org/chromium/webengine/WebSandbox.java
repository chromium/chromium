// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

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

import org.chromium.webengine.interfaces.IWebSandboxCallback;
import org.chromium.webengine.interfaces.IWebSandboxService;

import java.util.ArrayList;
import java.util.List;

/**
 * Handle to the browsing Sandbox. Must be created asynchronously.
 */
public class WebSandbox {
    // TODO(swestphal): Remove this and its function args and detect which service should be used
    // based on android version.
    private static final String BROWSER_PROCESS_MODE =
            "org.chromium.webengine.shell.BrowserProcessMode";

    // Use another APK as a placeholder for an actual sandbox, since they are conceptually the
    // same thing.
    private static final String SANDBOX_BROWSER_SANDBOX_PACKAGE = "org.chromium.webengine.sandbox";

    private static final String BROWSER_SANDBOX_ACTION =
            "org.chromium.weblayer.intent.action.BROWSERSANDBOX";
    private static final String BROWSER_INPROCESS_ACTION =
            "org.chromium.weblayer.intent.action.BROWSERINPROCESS";

    private static final String DEFAULT_PROFILE_NAME = "DefaultProfile";

    private static WebSandbox sInstance;
    private static boolean sPendingConnection;

    private ConnectionSetup mConnection;
    private IWebSandboxService mWebSandboxService;

    private List<WebFragment> mActiveFragments = new ArrayList<>();

    private static class ConnectionSetup implements ServiceConnection {
        private CallbackToFutureAdapter.Completer<WebSandbox> mCompleter;
        private IWebSandboxService mWebSandboxService;
        private Context mContext;

        private final IWebSandboxCallback mWebSandboxCallback = new IWebSandboxCallback.Stub() {
            @Override
            public void onBrowserProcessInitialized() {
                sInstance = new WebSandbox(ConnectionSetup.this, mWebSandboxService);
                sPendingConnection = false;
                mCompleter.set(sInstance);
                mCompleter = null;
            }
        };

        ConnectionSetup(Context context, CallbackToFutureAdapter.Completer<WebSandbox> completer) {
            mContext = context;
            mCompleter = completer;
        }

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            mWebSandboxService = IWebSandboxService.Stub.asInterface(service);
            try {
                mWebSandboxService.initializeBrowserProcess(mWebSandboxCallback);
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

    private WebSandbox(ConnectionSetup connection, IWebSandboxService service) {
        mConnection = connection;
        mWebSandboxService = service;
    }

    /**
     * Asynchronously creates a handle to the web sandbox after initializing the
     * browser process.
     * @param context The application context.
     */
    @NonNull
    public static ListenableFuture<WebSandbox> create(@NonNull Context context) {
        if (sInstance != null) {
            return Futures.immediateFuture(sInstance);
        }
        if (sPendingConnection) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox is already being created"));
        }
        sPendingConnection = true;
        return CallbackToFutureAdapter.getFuture(completer -> {
            // Use the application context since the WebSandbox might out live the Activity.
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
            return "WebSandbox Sandbox Future";
        });
    }

    /**
     * Creates a new WebFragment for displaying web content.
     */
    @Nullable
    public WebFragment createFragment() {
        FragmentParams params =
                (new FragmentParams.Builder()).setProfileName(DEFAULT_PROFILE_NAME).build();
        return createFragment(params);
    }

    /**
     * Creates a new WebFragment for displaying web content.
     */
    @Nullable
    public WebFragment createFragment(FragmentParams params) {
        if (mWebSandboxService == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            WebFragment fragment = new WebFragment();
            fragment.initialize(
                    this, mWebSandboxService.createFragmentDelegate(params.getParcelable()));
            return fragment;
        } catch (RemoteException e) {
            return null;
        }
    }

    /**
     * Enables or disables DevTools remote debugging.
     */
    public void setRemoteDebuggingEnabled(boolean enabled) {
        if (mWebSandboxService == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mWebSandboxService.setRemoteDebuggingEnabled(enabled);
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

    void addFragment(WebFragment fragment) {
        mActiveFragments.add(fragment);
    }

    void removeFragment(WebFragment fragment) {
        mActiveFragments.remove(fragment);
    }

    boolean isShutdown() {
        return mWebSandboxService == null;
    }

    public void shutdown() {
        if (isShutdown()) {
            // WebSandbox was already shut down.
            return;
        }

        for (WebFragment fragment : mActiveFragments) {
            // This will detach the fragment, save the state, and remove |fragment| from
            // |mActiveFragments|.
            fragment.invalidate();
        }

        mWebSandboxService = null;
        mConnection.unbind();
    }
}
