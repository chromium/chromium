// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.StrictMode;
import android.util.AndroidRuntimeException;
import android.util.Log;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.IWebLayerClient;
import org.chromium.weblayer_private.interfaces.IWebLayerFactory;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.interfaces.WebLayerVersionConstants;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/**
 * WebLayer is responsible for initializing state necessary to use any of the classes in web layer.
 */
@VisibleForTesting
public class WebLayer {
    private static final String TAG = "WebLayer";
    // This metadata key, if defined, overrides the default behaviour of loading WebLayer from the
    // current WebView implementation. This is only intended for testing, and does not enforce any
    // signature requirements on the implementation, nor does it use the production code path to
    // load the code. Do not set this in production APKs!
    private static final String PACKAGE_MANIFEST_KEY = "org.chromium.weblayer.WebLayerPackage";

    @SuppressWarnings("StaticFieldLeak")
    @Nullable
    private static Context sRemoteContext;

    @Nullable
    private static ClassLoader sRemoteClassLoader;

    @Nullable
    private static Context sAppContext;

    @Nullable
    private static WebLayerLoader sLoader;

    private static boolean sDisableWebViewCompatibilityMode;

    @NonNull
    private final IWebLayer mImpl;

    /**
     * Returns true if WebLayer is available. This tries to load WebLayer, but does no
     * initialization. This function may be called by code that uses WebView.
     * <p>
     * NOTE: it's possible for this to return true, yet loading to still fail. This happens if there
     * is an error during loading.
     *
     * @return true Returns true if WebLayer is available.
     */
    static boolean isAvailable(Context context) {
        ThreadCheck.ensureOnUiThread();
        return getWebLayerLoader(context).isAvailable();
    }

    private static void checkAvailable(Context context) {
        if (!isAvailable(context)) {
            throw new UnsupportedVersionException(sLoader.getVersion());
        }
    }

    /**
     * Asynchronously creates and initializes WebLayer. Calling this more than once returns the same
     * object. Both this method and {@link #loadSync} yield the same instance of {@link WebLayer}.
     * <p>
     * {@link callback} is supplied null if unable to load WebLayer. In general, the only time null
     * is supplied is if there is an unexpected error.
     *
     * @param appContext The hosting application's Context.
     * @param callback {@link Callback} which will receive the WebLayer instance.
     * @throws UnsupportedVersionException If {@link #isAvailable} returns false. See
     * {@link #isAvailable} for details.
     */
    static void loadAsync(@NonNull Context appContext, @NonNull Callback<WebLayer> callback)
            throws UnsupportedVersionException {
        ThreadCheck.ensureOnUiThread();
        checkAvailable(appContext);
        getWebLayerLoader(appContext).loadAsync(callback);
    }

    /**
     * Synchronously creates and initializes WebLayer.
     * Both this method and {@link #loadAsync} yield the same instance of {@link WebLayer}.
     * It is safe to call this method after {@link #loadAsync} to block until the ongoing load
     * finishes (or immediately return its result if already finished).
     * <p>
     * This returns null if unable to load WebLayer. In general, the only time null
     * is returns is if there is an unexpected error loading.
     *
     * @param appContext The hosting application's Context.
     * @return a {@link WebLayer} instance, or null if unable to load WebLayer.
     *
     * @throws UnsupportedVersionException If {@link #isAvailable} returns false. See
     * {@link #isAvailable} for details.
     */
    @Nullable
    static WebLayer loadSync(@NonNull Context appContext) throws UnsupportedVersionException {
        ThreadCheck.ensureOnUiThread();
        checkAvailable(appContext);
        return getWebLayerLoader(appContext).loadSync();
    }

    private static WebLayerLoader getWebLayerLoader(Context context) {
        if (sLoader == null) sLoader = new WebLayerLoader(context);
        return sLoader;
    }

    /** Returns whether WebLayer loading has at least started. */
    static boolean hasWebLayerInitializationStarted() {
        return sLoader != null;
    }

    IWebLayer getImpl() {
        return mImpl;
    }

    static WebLayer getLoadedWebLayer(@NonNull Context appContext)
            throws UnsupportedVersionException {
        ThreadCheck.ensureOnUiThread();
        checkAvailable(appContext);
        return getWebLayerLoader(appContext).getLoadedWebLayer();
    }

    /**
     * Returns the supported version. Using any functions defined in a newer version than
     * returned by {@link getSupportedMajorVersion} result in throwing an
     * UnsupportedOperationException.
     * <p> For example, consider the function {@link setBottomBar}, and further assume
     * {@link setBottomBar} was added in version 11. If {@link getSupportedMajorVersion}
     * returns 10, then calling {@link setBottomBar} returns in an UnsupportedOperationException.
     * OTOH, if {@link getSupportedMajorVersion} returns 12, then {@link setBottomBar} works as
     * expected
     *
     * @return the supported version, or -1 if WebLayer is not available.
     */
    static int getSupportedMajorVersion(@NonNull Context context) {
        ThreadCheck.ensureOnUiThread();
        return getWebLayerLoader(context).getMajorVersion();
    }

    // Returns true if version checks should be done. This is provided solely for testing, and
    // specifically testing that does not run on device and load the implementation. It is only
    // necessary to check this in code paths that don't require WebLayer to load the implementation
    // and need to be callable in tests.
    static boolean shouldPerformVersionChecks() {
        return !"robolectric".equals(Build.FINGERPRINT);
    }

    // Internal version of getSupportedMajorVersion(). This should only be used when you know
    // WebLayer has been initialized. Generally that means calling this from any non-static method.
    static int getSupportedMajorVersionInternal() {
        if (sLoader == null) {
            throw new IllegalStateException(
                    "This should only be called once WebLayer is initialized");
        }
        return sLoader.getMajorVersion();
    }

    // Internal getter for the app Context. This should only be used when you know WebLayer has
    // been initialized.
    static Context getAppContext() {
        return sAppContext;
    }

    /**
     * Returns the Chrome version of the WebLayer implementation. This will return a full version
     * string such as "79.0.3945.0", while {@link getSupportedMajorVersion} will only return the
     * major version integer (79 in the example).
     */
    @NonNull
    static String getSupportedFullVersion(@NonNull Context context) {
        ThreadCheck.ensureOnUiThread();
        return getWebLayerLoader(context).getVersion();
    }

    /**
     * Returns the Chrome version this client was built at. This will return a full version string
     * such as "79.0.3945.0".
     */
    @NonNull
    static String getVersion() {
        ThreadCheck.ensureOnUiThread();
        return WebLayerClientVersionConstants.PRODUCT_VERSION;
    }

    @NonNull
    static String getProviderPackageName(@NonNull Context context) {
        ThreadCheck.ensureOnUiThread();
        return getWebLayerLoader(context).getProviderPackageName();
    }

    /**
     * Encapsulates the state of WebLayer loading and initialization.
     */
    private static final class WebLayerLoader {
        @NonNull
        private final List<Callback<WebLayer>> mCallbacks = new ArrayList<>();
        @Nullable
        private IWebLayerFactory mFactory;
        @Nullable
        private IWebLayer mIWebLayer;
        @Nullable
        private WebLayer mWebLayer;
        // True if WebLayer is available and compatible with this client.
        private final boolean mAvailable;
        private final int mMajorVersion;
        private final String mVersion;
        private boolean mIsLoadingAsync;
        private Context mContext;

        /**
         * Creates WebLayerLoader. This does a minimal amount of loading
         */
        public WebLayerLoader(@NonNull Context context) {
            boolean available = false;
            int majorVersion = -1;
            String version = "<unavailable>";
            // Use the application context as the supplied context may have a shorter lifetime.
            mContext = context.getApplicationContext();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    && ApiHelperForR.getAttributionTag(context) != null) {
                // Getting the application context means we lose any attribution. Use the
                // attribution tag from the supplied context so that embedders have a way to set an
                // attribution tag.
                mContext = ApiHelperForR.createAttributionContext(
                        mContext, ApiHelperForR.getAttributionTag(context));
            }
            try {
                Class factoryClass = loadRemoteClass(
                        mContext, "org.chromium.weblayer_private.WebLayerFactoryImpl");
                mFactory = IWebLayerFactory.Stub.asInterface(
                        (IBinder) factoryClass
                                .getMethod("create", String.class, int.class, int.class)
                                .invoke(null, WebLayerClientVersionConstants.PRODUCT_VERSION,
                                        WebLayerClientVersionConstants.PRODUCT_MAJOR_VERSION, -1));
                available = mFactory.isClientSupported();
                majorVersion = mFactory.getImplementationMajorVersion();
                version = mFactory.getImplementationVersion();
                if (available) {
                    available = majorVersion >= WebLayerVersionConstants.MIN_VERSION;
                }
                // See comment in WebLayerFactoryImpl.isClientSupported() for details on this.
                if (available
                        && WebLayerClientVersionConstants.PRODUCT_MAJOR_VERSION > majorVersion) {
                    available = WebLayerClientVersionConstants.PRODUCT_MAJOR_VERSION - majorVersion
                            <= WebLayerVersionConstants.MAX_SKEW;
                }
            } catch (Exception e) {
                Log.e(TAG, "Unable to create WebLayerFactory", e);
            }
            mAvailable = available;
            mMajorVersion = majorVersion;
            mVersion = version;
        }

        public String getProviderPackageName() {
            try {
                return getOrCreateRemoteContext(mContext).getPackageName();
            } catch (Exception e) {
                throw new APICallException(e);
            }
        }

        public boolean isAvailable() {
            return mAvailable;
        }

        public int getMajorVersion() {
            return mMajorVersion;
        }

        public String getVersion() {
            return mVersion;
        }

        public void loadAsync(@NonNull Callback<WebLayer> callback) {
            if (mWebLayer != null) {
                callback.onResult(mWebLayer);
                return;
            }
            mCallbacks.add(callback);
            if (mIsLoadingAsync) {
                return; // Already loading.
            }
            mIsLoadingAsync = true;
            if (getIWebLayer() == null) {
                // Unable to create WebLayer. This generally shouldn't happen.
                onWebLayerReady();
                return;
            }
            try {
                getIWebLayer().loadAsync(ObjectWrapper.wrap(mContext),
                        ObjectWrapper.wrap(getOrCreateRemoteContext(mContext)),
                        ObjectWrapper.wrap(
                                (ValueCallback<Boolean>) result -> { onWebLayerReady(); }));
            } catch (Exception e) {
                throw new APICallException(e);
            }
        }

        public WebLayer loadSync() {
            if (mWebLayer != null) {
                return mWebLayer;
            }
            if (getIWebLayer() == null) {
                // Error in creating WebLayer. This generally shouldn't happen.
                onWebLayerReady();
                return null;
            }
            try {
                getIWebLayer().loadSync(ObjectWrapper.wrap(mContext),
                        ObjectWrapper.wrap(getOrCreateRemoteContext(mContext)));
                onWebLayerReady();
                return mWebLayer;
            } catch (Exception e) {
                throw new APICallException(e);
            }
        }

        WebLayer getLoadedWebLayer() {
            return mWebLayer;
        }

        @Nullable
        private IWebLayer getIWebLayer() {
            if (mIWebLayer != null) return mIWebLayer;
            if (!mAvailable) return null;
            try {
                mIWebLayer = mFactory.createWebLayer();
            } catch (RemoteException e) {
                // If |mAvailable| returns true, then create() should always succeed.
                throw new AndroidRuntimeException(e);
            }
            return mIWebLayer;
        }

        private void onWebLayerReady() {
            if (mWebLayer != null) {
                return;
            }
            if (mIWebLayer != null) mWebLayer = new WebLayer(mIWebLayer);
            for (Callback<WebLayer> callback : mCallbacks) {
                callback.onResult(mWebLayer);
            }
            mCallbacks.clear();
        }
    }

    // Constructor for test mocking.
    protected WebLayer() {
        mImpl = null;
    }

    private WebLayer(IWebLayer iWebLayer) {
        mImpl = iWebLayer;

        try {
            mImpl.setClient(new WebLayerClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Get or create the profile for profileName.
     * @param profileName Null to indicate in-memory profile. Otherwise, name cannot be empty
     * and should contain only alphanumeric and underscore characters since it will be used as
     * a directory name in the file system.
     */
    @NonNull
    public Profile getProfile(@Nullable String profileName) {
        ThreadCheck.ensureOnUiThread();
        IProfile iprofile;
        try {
            iprofile = mImpl.getProfile(Profile.sanitizeProfileName(profileName));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return Profile.of(iprofile);
    }

    /**
     * Get or create the incognito profile with the name {@link profileName}.
     *
     * @param profileName The name of the profile. Null is mapped to an empty string.
     */
    @NonNull
    public Profile getIncognitoProfile(@Nullable String profileName) {
        ThreadCheck.ensureOnUiThread();
        IProfile iprofile;
        try {
            iprofile = mImpl.getIncognitoProfile(Profile.sanitizeProfileName(profileName));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return Profile.of(iprofile);
    }

    /**
     * Return a list of Profile names currently on disk. This does not include incognito
     * profiles. This will not include profiles that are being deleted from disk.
     * WebLayer must be initialized before calling this.
     */
    public void enumerateAllProfileNames(@NonNull Callback<String[]> callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            ValueCallback<String[]> valueCallback = (String[] value) -> callback.onResult(value);
            mImpl.enumerateAllProfileNames(ObjectWrapper.wrap(valueCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the user agent string used by WebLayer.
     *
     * @return The user-agent string.
     */
    public String getUserAgentString() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mImpl.getUserAgentString();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * To enable or disable DevTools remote debugging.
     */
    public void setRemoteDebuggingEnabled(boolean enabled) {
        try {
            mImpl.setRemoteDebuggingEnabled(enabled);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * @return Whether or not DevTools remote debugging is enabled.
     */
    public boolean isRemoteDebuggingEnabled() {
        try {
            return mImpl.isRemoteDebuggingEnabled();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Provide WebLayer with a set of active external experiment IDs.
     *
     * These experiment IDs are to be incorporated into metrics collection performed by WebLayer
     * to aid in interpretation of data and elimination of confounding factors.
     *
     * This method may be called multiple times to update experient IDs if they change.
     *
     * @param experimentIds An array of integer active experiment IDs relevant to WebLayer.
     */
    public void registerExternalExperimentIDs(@NonNull int[] experimentIds) {
        ThreadCheck.ensureOnUiThread();
        try {
            // First param no longer used. First param was not used in the backend as of 85, and
            // always supplied as empty as of 89 client.
            mImpl.registerExternalExperimentIDs("", experimentIds);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the value of X-Client-Data header.
     * @since 101
     */
    public String getXClientDataHeader() {
        ThreadCheck.ensureOnUiThread();
        if (getSupportedMajorVersionInternal() < 101) {
            throw new UnsupportedOperationException();
        }
        try {
            return mImpl.getXClientDataHeader();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    IBrowser createBrowser(Context serviceContext, Bundle fragmentArgs) {
        try {
            return mImpl.createBrowser(
                    ObjectWrapper.wrap(serviceContext), ObjectWrapper.wrap(fragmentArgs));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /* package */ static IWebLayer getIWebLayer(Context context) {
        return getWebLayerLoader(context).getIWebLayer();
    }

    /* package */ static Context getApplicationContextForTesting(Context appContext) {
        try {
            return (Context) ObjectWrapper.unwrap(
                    getIWebLayer(appContext).getApplicationContext(), Context.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Creates a ClassLoader for the remote (weblayer implementation) side.
     */
    static Class<?> loadRemoteClass(Context appContext, String className)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        if (sRemoteClassLoader != null) {
            return sRemoteClassLoader.loadClass(className);
        }

        // Child processes do not need WebView compatibility since there is no chance
        // WebView will run in the same process.
        if (sDisableWebViewCompatibilityMode) {
            Context context = getOrCreateRemoteContext(appContext);
            // Android versions before O do not support isolated splits, so WebLayer will be loaded
            // as a normal split which is already available from the base class loader.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                // Attempt to find the class in the base ClassLoader, if it doesn't exist then load
                // the weblayer ClassLoader.
                try {
                    Class.forName(className, false, context.getClassLoader());
                } catch (ClassNotFoundException e) {
                    try {
                        // If the implementation APK does not support isolated splits, this will
                        // just return the original context.
                        context = ApiHelperForO.createContextForSplit(context, "weblayer");
                    } catch (PackageManager.NameNotFoundException e2) {
                        // WebLayer not in split, proceed with the base context.
                    }
                }
            }
            sRemoteClassLoader = context.getClassLoader();
        } else {
            sRemoteClassLoader = WebViewCompatibilityHelper.initialize(appContext);
        }
        return sRemoteClassLoader.loadClass(className);
    }

    /**
     * Creates a Context for the remote (weblayer implementation) side.
     */
    static Context getOrCreateRemoteContext(Context appContext)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        if (sRemoteContext != null) {
            return sRemoteContext;
        }
        Class<?> webViewFactoryClass = Class.forName("android.webkit.WebViewFactory");
        String implPackageName = getImplPackageName(appContext);
        sAppContext = appContext;
        if (implPackageName != null) {
            sRemoteContext = createRemoteContextFromPackageName(appContext, implPackageName);
        } else {
            Method getContext =
                    webViewFactoryClass.getDeclaredMethod("getWebViewContextAndSetProvider");
            getContext.setAccessible(true);
            sRemoteContext = (Context) getContext.invoke(null);
        }
        return sRemoteContext;
    }

    /* package */ static void disableWebViewCompatibilityMode() {
        sDisableWebViewCompatibilityMode = true;
    }

    /**
     * Creates a Context for the remote (weblayer implementation) side
     * using a specified package name as the implementation. This is only
     * intended for testing, not production use.
     */
    private static Context createRemoteContextFromPackageName(
            Context appContext, String implPackageName)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        // Load the code for the target package.
        Context remoteContext = appContext.createPackageContext(
                implPackageName, Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);

        // Get the package info for the target package.
        PackageInfo implPackageInfo = appContext.getPackageManager().getPackageInfo(implPackageName,
                PackageManager.GET_SHARED_LIBRARY_FILES | PackageManager.GET_META_DATA);

        // Store this package info in WebViewFactory as if it had been loaded as WebView,
        // because other parts of the implementation need to be able to fetch it from there.
        Class<?> webViewFactory = Class.forName("android.webkit.WebViewFactory");
        Field sPackageInfo = webViewFactory.getDeclaredField("sPackageInfo");
        sPackageInfo.setAccessible(true);
        sPackageInfo.set(null, implPackageInfo);

        return remoteContext;
    }

    private static String getImplPackageName(Context appContext)
            throws PackageManager.NameNotFoundException {
        Bundle metaData = appContext.getPackageManager()
                                  .getApplicationInfo(
                                          appContext.getPackageName(), PackageManager.GET_META_DATA)
                                  .metaData;
        if (metaData != null) return metaData.getString(PACKAGE_MANIFEST_KEY);
        return null;
    }

    private final class WebLayerClientImpl extends IWebLayerClient.Stub {
        @Override
        public Intent createIntent() {
            StrictModeWorkaround.apply();
            // Intent objects need to be created in the client library so they can refer to the
            // broadcast receiver that will handle them. The broadcast receiver needs to be in the
            // client library because it's referenced in the manifest.
            return new Intent(WebLayer.getAppContext(), BroadcastReceiver.class);
        }

        @Override
        public Intent createMediaSessionServiceIntent() {
            StrictModeWorkaround.apply();
            return new Intent(WebLayer.getAppContext(), MediaSessionService.class);
        }

        @Override
        public Intent createImageDecoderServiceIntent() {
            StrictModeWorkaround.apply();
            return new Intent(WebLayer.getAppContext(), ImageDecoderService.class);
        }

        @Override
        public int getMediaSessionNotificationId() {
            StrictModeWorkaround.apply();
            // The id is part of the public library to avoid conflicts.
            return R.id.weblayer_media_session_notification;
        }

        @Override
        public Intent createRemoteMediaServiceIntent() {
            StrictModeWorkaround.apply();
            return new Intent(WebLayer.getAppContext(), RemoteMediaService.class);
        }

        @Override
        public int getPresentationApiNotificationId() {
            StrictModeWorkaround.apply();
            // The id is part of the public library to avoid conflicts.
            return R.id.weblayer_presentation_api_notification;
        }

        @Override
        public int getRemotePlaybackApiNotificationId() {
            StrictModeWorkaround.apply();
            // The id is part of the public library to avoid conflicts.
            return R.id.weblayer_remote_playback_api_notification;
        }

        @Override
        public int getMaxNavigationsPerTabForInstanceState() {
            return Browser.getMaxNavigationsPerTabForInstanceState();
        }
    }

    /** Utility class to use new APIs that were added in O (API level 26). */
    @VerifiesOnO
    @RequiresApi(Build.VERSION_CODES.O)
    /* package */ static final class ApiHelperForO {
        /** See {@link Context.createContextForSplit(String) }. */
        public static Context createContextForSplit(Context context, String name)
                throws PackageManager.NameNotFoundException {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                return context.createContextForSplit(name);
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }

        /** See {@link ApplicationInfo#splitNames}. */
        public static String[] getSplitNames(ApplicationInfo info) {
            return info.splitNames;
        }
    }

    @VerifiesOnR
    @RequiresApi(Build.VERSION_CODES.R)
    private static final class ApiHelperForR {
        /** See {@link Context.getAttributionTag() }. */
        public static String getAttributionTag(Context context) {
            return context.getAttributionTag();
        }

        /** See {@link Context.createAttributionContext(String) }. */
        public static Context createAttributionContext(Context context, String tag) {
            return context.createAttributionContext(tag);
        }
    }
}
