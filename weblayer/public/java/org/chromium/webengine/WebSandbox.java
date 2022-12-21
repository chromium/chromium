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
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.ITabManagerDelegate;
import org.chromium.webengine.interfaces.IWebEngineDelegate;
import org.chromium.webengine.interfaces.IWebEngineDelegateClient;
import org.chromium.webengine.interfaces.IWebFragmentEventsDelegate;
import org.chromium.webengine.interfaces.IWebSandboxCallback;
import org.chromium.webengine.interfaces.IWebSandboxService;

import java.util.ArrayList;
import java.util.List;

/**
 * Handle to the browsing Sandbox. Must be created asynchronously.
 */
public class WebSandbox {
    private final Handler mHandler = new Handler(Looper.getMainLooper());
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

    private List<WebEngine> mActiveWebEngines = new ArrayList<>();

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

    private class WebEngineDelegateClient extends IWebEngineDelegateClient.Stub {
        private CallbackToFutureAdapter.Completer<WebEngine> mWebEngineCompleter;

        WebEngineDelegateClient(CallbackToFutureAdapter.Completer<WebEngine> completer) {
            mWebEngineCompleter = completer;
        }

        @Override
        public void onDelegatesReady(IWebEngineDelegate delegate,
                IWebFragmentEventsDelegate fragmentEventsDelegate,
                ITabManagerDelegate tabManagerDelegate,
                ICookieManagerDelegate cookieManagerDelegate) {
            mHandler.post(() -> {
                WebEngine engine = WebEngine.create(WebSandbox.this, delegate,
                        fragmentEventsDelegate, tabManagerDelegate, cookieManagerDelegate);
                engine.initializeTabManager((v) -> {
                    addWebEngine(engine);
                    mWebEngineCompleter.set(engine);
                });
            });
        }
    }

    /**
     * Asynchronously creates a new WebEngine with default Profile.
     */
    @Nullable
    public ListenableFuture<WebEngine> createWebEngine() {
        if (mWebSandboxService == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        FragmentParams params =
                (new FragmentParams.Builder()).setProfileName(DEFAULT_PROFILE_NAME).build();
        return createWebEngine(params);
    }

    /**
     * Asynchronously creates a new WebEngine.
     */
    @Nullable
    public ListenableFuture<WebEngine> createWebEngine(FragmentParams params) {
        if (mWebSandboxService == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        ListenableFuture<WebEngine> futureWebEngine =
                CallbackToFutureAdapter.getFuture(completer -> {
                    mWebSandboxService.createWebEngineDelegate(
                            params.getParcelable(), new WebEngineDelegateClient(completer));

                    return "WebEngineClient Future";
                });
        return futureWebEngine;
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

    void addWebEngine(WebEngine webEngine) {
        mActiveWebEngines.add(webEngine);
    }

    void removeWebEngine(WebEngine webEngine) {
        mActiveWebEngines.remove(webEngine);
    }

    /**
     * Returns all active {@link WebEngine}.
     */
    public List<WebEngine> getWebEngines() {
        return mActiveWebEngines;
    }

    boolean isShutdown() {
        return mWebSandboxService == null;
    }

    public void shutdown() {
        if (isShutdown()) {
            // WebSandbox was already shut down.
            return;
        }
        mWebSandboxService = null;

        for (WebEngine engine : mActiveWebEngines) {
            // This will shut down the WebEngine, its fragment, and remove {@code engine} from
            // {@code mActiveWebEngines}.
            engine.invalidate();
        }
        mActiveWebEngines.clear();

        mConnection.unbind();
    }
}
