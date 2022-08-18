// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.interfaces.IBrowserSandboxCallback;
import org.chromium.browserfragment.interfaces.IBrowserSandboxService;

/**
 * Handle to the Browsing Sandbox. Must be created asynchronously.
 */
public class Browser {
    // Use another APK as a placeholder for an actual sandbox, since they are conceptually the
    // same thing.
    private static final String BROWSER_SANDBOX_PACKAGE = "org.chromium.browserfragment.sandbox";

    private static final String BROWSER_SANDBOX_ACTION =
            "org.chromium.weblayer.intent.action.BROWSERSANDBOX";

    private static final String DEFAULT_PROFILE_NAME = "DefaultProfile";

    private static Browser sInstance;

    private IBrowserSandboxService mBrowserSandboxService;

    private static class ConnectionSetup implements ServiceConnection {
        private CallbackToFutureAdapter.Completer<Browser> mCompleter;
        private IBrowserSandboxService mBrowserSandboxService;
        private Context mContext;

        private final IBrowserSandboxCallback mBrowserSandboxCallback =
                new IBrowserSandboxCallback.Stub() {
                    @Override
                    public void onBrowserProcessInitialized() {
                        sInstance = new Browser(mBrowserSandboxService);
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

        // TODO(rayankans): Actually handle failure / disconnection events.
        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private Browser(IBrowserSandboxService service) {
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
        return CallbackToFutureAdapter.getFuture(completer -> {
            ConnectionSetup connectionSetup = new ConnectionSetup(context, completer);

            Intent intent = new Intent(BROWSER_SANDBOX_ACTION);
            intent.setPackage(BROWSER_SANDBOX_PACKAGE);

            context.bindService(intent, connectionSetup, Context.BIND_AUTO_CREATE);

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
        try {
            mBrowserSandboxService.setRemoteDebuggingEnabled(enabled);
        } catch (RemoteException e) {
        }
    }
}
