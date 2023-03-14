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
import androidx.core.content.ContextCompat;

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

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

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

    private Map<String, WebEngine> mActiveWebEngines = new HashMap<String, WebEngine>();

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

                    @Override
                    public void onBrowserProcessInitializationFailure() {
                        mCompleter.setException(
                                new IllegalStateException("Failed to initialize WebSandbox"));
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
            sSandboxConnectionInstance = null;
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
                ContextCompat.getMainExecutor(applicationContext));
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

        return Futures.transformAsync(sandboxConnectionFuture, isAvailableTask,
                ContextCompat.getMainExecutor(applicationContext));
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

        return Futures.transformAsync(sandboxConnectionFuture, getVersionTask,
                ContextCompat.getMainExecutor(applicationContext));
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
                ContextCompat.getMainExecutor(applicationContext));
    }

    private class WebEngineDelegateClient extends IWebEngineDelegateClient.Stub {
        private CallbackToFutureAdapter.Completer<WebEngine> mWebEngineCompleter;
        private String mTag;

        WebEngineDelegateClient(
                CallbackToFutureAdapter.Completer<WebEngine> completer, String tag) {
            mWebEngineCompleter = completer;
            mTag = tag;
        }

        @Override
        public void onDelegatesReady(IWebEngineDelegate delegate,
                IWebFragmentEventsDelegate fragmentEventsDelegate,
                ITabManagerDelegate tabManagerDelegate,
                ICookieManagerDelegate cookieManagerDelegate) {
            mHandler.post(() -> {
                WebEngine engine = WebEngine.create(WebSandbox.this, delegate,
                        fragmentEventsDelegate, tabManagerDelegate, cookieManagerDelegate, mTag);
                engine.initializeTabManager((v) -> {
                    addWebEngine(mTag, engine);
                    mWebEngineCompleter.set(engine);
                });
            });
        }
    }

    private String createNewTag() {
        int webEngineIndex = mActiveWebEngines.size();
        String tag = String.format("webengine_%d", webEngineIndex);

        while (mActiveWebEngines.containsKey(tag)) {
            ++webEngineIndex;
            tag = String.format("webengine_%d", webEngineIndex);
        }
        return tag;
    }

    /**
     * Asynchronously creates a new WebEngine with default Profile.
     */
    @Nullable
    public ListenableFuture<WebEngine> createWebEngine() {
        ThreadCheck.ensureOnUiThread();
        return createWebEngine(createNewTag());
    }

    /**
     * Asynchronously creates a new WebEngine with default Profile and gives it a {@code tag}.
     */
    @Nullable
    public ListenableFuture<WebEngine> createWebEngine(String tag) {
        ThreadCheck.ensureOnUiThread();
        if (mActiveWebEngines.containsKey(tag)) {
            throw new IllegalArgumentException("Tag already associated with a WebEngine");
        }
        if (mWebSandboxService == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        WebEngineParams params =
                (new WebEngineParams.Builder()).setProfileName(DEFAULT_PROFILE_NAME).build();
        return createWebEngine(params, tag);
    }

    /**
     * Asynchronously creates a new WebEngine based on {@code params}.
     */
    @Nullable
    public ListenableFuture<WebEngine> createWebEngine(WebEngineParams params) {
        ThreadCheck.ensureOnUiThread();
        return createWebEngine(params, createNewTag());
    }

    /**
     * Asynchronously creates a new WebEngine based on {@code params} and gives it a {@code tag}.
     */
    public ListenableFuture<WebEngine> createWebEngine(WebEngineParams params, String tag) {
        if (mActiveWebEngines.containsKey(tag)) {
            throw new IllegalArgumentException("Tag already associated with a WebEngine");
        }
        if (mWebSandboxService == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        ListenableFuture<WebEngine> futureWebEngine =
                CallbackToFutureAdapter.getFuture(completer -> {
                    mWebSandboxService.createWebEngineDelegate(
                            params.getParcelable(), new WebEngineDelegateClient(completer, tag));

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

    void addWebEngine(String tag, WebEngine webEngine) {
        assert !mActiveWebEngines.containsKey(tag) : "Key already associated with a WebEngine";
        mActiveWebEngines.put(tag, webEngine);
    }

    void removeWebEngine(String tag, WebEngine webEngine) {
        assert webEngine == mActiveWebEngines.get(tag);
        mActiveWebEngines.remove(tag);
    }

    /**
     * Returns the WebEngine with the given tag, or null if no WebEngine exists with this tag.
     *
     * @param tag the name of the WebEngine.
     *
     * @return WebEgine with the given tag.
     *
     */
    @Nullable
    public WebEngine getWebEngine(String tag) {
        return mActiveWebEngines.get(tag);
    }

    /**
     * Returns all active {@link WebEngine}.
     */
    public Collection<WebEngine> getWebEngines() {
        return mActiveWebEngines.values();
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

        for (WebEngine engine : mActiveWebEngines.values()) {
            // This will shut down the WebEngine, its fragment, and remove {@code engine} from
            // {@code mActiveWebEngines}.
            engine.invalidate();
        }
        mActiveWebEngines.clear();

        mConnection.unbind();
    }
}
