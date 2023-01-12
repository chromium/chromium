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

import com.google.common.util.concurrent.AsyncFunction;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.IStringCallback;
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
    private IWebSandboxService mWebSandboxService;

    private SandboxConnection mConnection;

    private List<WebEngine> mActiveWebEngines = new ArrayList<>();

    private static class SandboxConnection implements ServiceConnection {
        private static ListenableFuture<SandboxConnection> sSandboxConnectionFuture;
        private static SandboxConnection sSandboxConnectionInstance;

        private CallbackToFutureAdapter.Completer<SandboxConnection> mSandboxConnectionCompleter;
        private IWebSandboxService mWebSandboxService;

        private Context mContext;

        private static boolean sPendingBrowserProcessInitialization;

        private SandboxConnection(
                Context context, CallbackToFutureAdapter.Completer<SandboxConnection> completer) {
            mContext = context;
            mSandboxConnectionCompleter = completer;

            Intent intent = new Intent(
                    isInProcessMode(mContext) ? BROWSER_INPROCESS_ACTION : BROWSER_SANDBOX_ACTION);
            intent.setPackage(isInProcessMode(mContext) ? mContext.getPackageName()
                                                        : SANDBOX_BROWSER_SANDBOX_PACKAGE);
            mContext.bindService(intent, this, Context.BIND_AUTO_CREATE);
        }

        static ListenableFuture<SandboxConnection> getInstance(Context context) {
            if (sSandboxConnectionInstance != null) {
                return Futures.immediateFuture(sSandboxConnectionInstance);
            }
            if (sSandboxConnectionFuture == null) {
                sSandboxConnectionFuture = CallbackToFutureAdapter.getFuture(completer -> {
                    new SandboxConnection(context, completer);
                    return "SandboxConnection Future";
                });
            }
            return sSandboxConnectionFuture;
        }

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            mWebSandboxService = IWebSandboxService.Stub.asInterface(service);

            sSandboxConnectionInstance = this;
            sSandboxConnectionFuture = null;

            mSandboxConnectionCompleter.set(sSandboxConnectionInstance);
            mSandboxConnectionCompleter = null;
        }

        private CallbackToFutureAdapter.Completer<WebSandbox> mCompleter;

        void initializeBrowserProcess(CallbackToFutureAdapter.Completer<WebSandbox> completer) {
            assert !sPendingBrowserProcessInitialization
                : "SandboxInitialization already in progress";

            mCompleter = completer;

            sPendingBrowserProcessInitialization = true;
            try {
                mWebSandboxService.initializeBrowserProcess(new IWebSandboxCallback.Stub() {
                    @Override
                    public void onBrowserProcessInitialized() {
                        sInstance = new WebSandbox(SandboxConnection.this, mWebSandboxService);

                        mCompleter.set(sInstance);
                        mCompleter = null;

                        sPendingBrowserProcessInitialization = false;
                    }
                });
            } catch (RemoteException e) {
                mCompleter.setException(e);
                mCompleter = null;
            }
        }

        void isAvailable(CallbackToFutureAdapter.Completer<Boolean> completer)
                throws RemoteException {
            mWebSandboxService.isAvailable(new IBooleanCallback.Stub() {
                @Override
                public void onResult(boolean isAvailable) {
                    completer.set(isAvailable);
                }
                @Override
                public void onException(int type, String msg) {
                    completer.setException(ExceptionHelper.createException(type, msg));
                }
            });
        }

        void getVersion(CallbackToFutureAdapter.Completer<String> completer)
                throws RemoteException {
            mWebSandboxService.getVersion(new IStringCallback.Stub() {
                @Override
                public void onResult(String version) {
                    completer.set(version);
                }
                @Override
                public void onException(int type, String msg) {
                    completer.setException(ExceptionHelper.createException(type, msg));
                }
            });
        }

        void getProviderPackageName(CallbackToFutureAdapter.Completer<String> completer)
                throws RemoteException {
            mWebSandboxService.getProviderPackageName(new IStringCallback.Stub() {
                @Override
                public void onResult(String providerPackageName) {
                    completer.set(providerPackageName);
                }
                @Override
                public void onException(int type, String msg) {
                    completer.setException(ExceptionHelper.createException(type, msg));
                }
            });
        }

        void unbind() {
            mContext.unbindService(this);
            sInstance = null;
        }

        // TODO(rayankans): Actually handle failure / disconnection events.
        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private WebSandbox(SandboxConnection connection, IWebSandboxService service) {
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
        ThreadCheck.ensureOnUiThread();
        if (sInstance != null) {
            return Futures.immediateFuture(sInstance);
        }

        if (SandboxConnection.sPendingBrowserProcessInitialization) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox is already being created"));
        }
        Context applicationContext = context.getApplicationContext();
        ListenableFuture<SandboxConnection> sandboxConnectionFuture =
                SandboxConnection.getInstance(applicationContext);

        AsyncFunction<SandboxConnection, WebSandbox> initializeBrowserProcessTask =
                sandboxConnection -> {
            return CallbackToFutureAdapter.getFuture(completer -> {
                sandboxConnection.initializeBrowserProcess(completer);

                // Debug string.
                return "WebSandbox Sandbox Future";
            });
        };

        return Futures.transformAsync(sandboxConnectionFuture, initializeBrowserProcessTask,
                applicationContext.getMainExecutor());
    }

    @NonNull
    public static ListenableFuture<Boolean> isAvailable(@NonNull Context context) {
        ThreadCheck.ensureOnUiThread();
        Context applicationContext = context.getApplicationContext();
        ListenableFuture<SandboxConnection> sandboxConnectionFuture =
                SandboxConnection.getInstance(applicationContext);

        AsyncFunction<SandboxConnection, Boolean> isAvailableTask = sandboxConnection -> {
            return CallbackToFutureAdapter.getFuture(completer -> {
                sandboxConnection.isAvailable(completer);

                // Debug string.
                return "Sandbox Available Future";
            });
        };

        return Futures.transformAsync(
                sandboxConnectionFuture, isAvailableTask, applicationContext.getMainExecutor());
    }

    @NonNull
    public static ListenableFuture<String> getVersion(@NonNull Context context) {
        ThreadCheck.ensureOnUiThread();
        Context applicationContext = context.getApplicationContext();
        ListenableFuture<SandboxConnection> sandboxConnectionFuture =
                SandboxConnection.getInstance(applicationContext);

        AsyncFunction<SandboxConnection, String> getVersionTask = sandboxConnection -> {
            return CallbackToFutureAdapter.getFuture(completer -> {
                sandboxConnection.getVersion(completer);

                // Debug string.
                return "Sandbox Version Future";
            });
        };

        return Futures.transformAsync(
                sandboxConnectionFuture, getVersionTask, applicationContext.getMainExecutor());
    }

    @NonNull
    public static ListenableFuture<String> getProviderPackageName(@NonNull Context context) {
        ThreadCheck.ensureOnUiThread();
        Context applicationContext = context.getApplicationContext();
        ListenableFuture<SandboxConnection> sandboxConnectionFuture =
                SandboxConnection.getInstance(applicationContext);

        AsyncFunction<SandboxConnection, String> getProviderPackageNameTask = sandboxConnection -> {
            return CallbackToFutureAdapter.getFuture(completer -> {
                sandboxConnection.getProviderPackageName(completer);

                // Debug string.
                return "Sandbox Provider Package Future";
            });
        };

        return Futures.transformAsync(sandboxConnectionFuture, getProviderPackageNameTask,
                applicationContext.getMainExecutor());
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
        ThreadCheck.ensureOnUiThread();
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
        ThreadCheck.ensureOnUiThread();
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
        ThreadCheck.ensureOnUiThread();
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
