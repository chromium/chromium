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
import android.view.SurfaceControlViewHost.SurfacePackage;
import android.view.SurfaceView;

import androidx.concurrent.futures.CallbackToFutureAdapter;

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

    private IBrowserSandboxService mBrowserSandboxService;
    private SurfaceView mSurfaceView;

    private final IBrowserSandboxCallback mBrowserSandboxCallback =
            new IBrowserSandboxCallback.Stub() {
                @Override
                public void onSurfacePackageReady(SurfacePackage surfacePackage) {
                    mSurfaceView.setChildSurfacePackage(surfacePackage);
                }
            };

    private static class ConnectionSetup implements ServiceConnection {
        private CallbackToFutureAdapter.Completer<Browser> mCompleter;
        private Browser mBrowser;
        private Context mContext;

        ConnectionSetup(Context context, CallbackToFutureAdapter.Completer<Browser> completer) {
            mContext = context;
            mCompleter = completer;
        }

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            IBrowserSandboxService browserSandboxService =
                    IBrowserSandboxService.Stub.asInterface(service);

            // TODO(rayankans): Initialize the browser process in the Browser Sandbox before
            // resolving the promise.
            mBrowser = new Browser(browserSandboxService);
            mCompleter.set(mBrowser);
            mCompleter = null;
        }

        // TODO(rayankans): Actually handle failure / disconnection events.
        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private Browser(IBrowserSandboxService service) {
        mBrowserSandboxService = service;
    }

    public static ListenableFuture<Browser> create(Context context) {
        return CallbackToFutureAdapter.getFuture(completer -> {
            ConnectionSetup connectionSetup = new ConnectionSetup(context, completer);

            Intent intent = new Intent(BROWSER_SANDBOX_ACTION);
            intent.setPackage(BROWSER_SANDBOX_PACKAGE);

            context.bindService(intent, connectionSetup, Context.BIND_AUTO_CREATE);

            // Debug string.
            return "Browser Sandbox Future";
        });
    }

    public void attachViewHierarchy(SurfaceView surfaceView) {
        mSurfaceView = surfaceView;
        mSurfaceView.setZOrderOnTop(true);
        try {
            mBrowserSandboxService.attachViewHierarchy(
                    mSurfaceView.getHostToken(), mBrowserSandboxCallback);
        } catch (RemoteException e) {
        }
    }
}
