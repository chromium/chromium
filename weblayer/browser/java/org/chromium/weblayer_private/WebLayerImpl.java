// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.annotation.SuppressLint;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.AndroidRuntimeException;
import android.util.SparseArray;
import android.webkit.ValueCallback;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;

import androidx.annotation.Nullable;
import androidx.core.content.FileProvider;

import dalvik.system.DexFile;

import org.chromium.base.BuildInfo;
import org.chromium.base.BundleUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.components.browser_ui.contacts_picker.ContactsPickerDialog;
import org.chromium.components.browser_ui.photo_picker.DecoderServiceHost;
import org.chromium.components.browser_ui.photo_picker.ImageDecoder;
import org.chromium.components.browser_ui.photo_picker.PhotoPickerDelegateBase;
import org.chromium.components.browser_ui.photo_picker.PhotoPickerDialog;
import org.chromium.components.browser_ui.share.ClipboardImageFileProvider;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.component_updater.ComponentLoaderPolicyBridge;
import org.chromium.components.component_updater.EmbeddedComponentLoader;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.components.embedder_support.application.FirebaseConfig;
import org.chromium.components.payments.PaymentDetailsUpdateService;
import org.chromium.components.webapk.lib.client.ChromeWebApkHostSignature;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;
import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PhotoPicker;
import org.chromium.ui.base.PhotoPickerListener;
import org.chromium.ui.base.ResourceBundle;
import org.chromium.ui.base.SelectFileDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.IWebLayerClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.media.MediaRouterClientImpl;
import org.chromium.weblayer_private.media.MediaSessionManager;
import org.chromium.weblayer_private.media.MediaStreamManager;
import org.chromium.weblayer_private.metrics.MetricsServiceClient;
import org.chromium.weblayer_private.metrics.UmaUtils;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Root implementation class for WebLayer.
 */
@JNINamespace("weblayer")
public final class WebLayerImpl extends IWebLayer.Stub {
    // TODO: should there be one tag for all this code?
    private static final String TAG = "WebLayer";
    private static final String PRIVATE_DIRECTORY_SUFFIX = "weblayer";
    // Command line flags are only read in debug builds.
    // WARNING: this file is written to by testing code in chrome (see
    // "//chrome/test/chromedriver/chrome/device_manager.cc"). If you change this variable, update
    // "device_manager.cc" too. If the command line file exists in the app's private files
    // directory, it will be read from there, otherwise it will be read from
    // PUBLIC_COMMAND_LINE_FILE.
    private static final String COMMAND_LINE_FILE = "weblayer-command-line";
    private static final String PUBLIC_COMMAND_LINE_FILE = "/data/local/tmp/" + COMMAND_LINE_FILE;
    // This metadata key, if defined, overrides the default behaviour of loading WebLayer from the
    // current WebView implementation. This is only intended for testing, and does not enforce any
    // signature requirements on the implementation, nor does it use the production code path to
    // load the code. Do not set this in production APKs!
    private static final String PACKAGE_MANIFEST_KEY = "org.chromium.weblayer.WebLayerPackage";
    // SharedPreferences key storing the versionCode of the most recently loaded WebLayer library.
    public static final String PREF_LAST_VERSION_CODE =
            "org.chromium.weblayer.last_version_code_used";

    // The required package ID for WebLayer when loaded as a shared library, hardcoded in the
    // resources. If this value changes make sure to change _SHARED_LIBRARY_HARDCODED_ID in
    // //build/android/gyp/util/protoresources.py and WebViewChromiumFactoryProvider.java.
    private static final int REQUIRED_PACKAGE_IDENTIFIER = 36;

    // 0 results in using the default value.
    private static int sMaxNavigationsForInstanceState = 0;

    private final ProfileManager mProfileManager = new ProfileManager();

    private boolean mInited;
    private static IWebLayerClient sClient;

    // Whether WebView is running in process. Set in init().
    private boolean mIsWebViewCompatMode;

    private boolean mOnNativeLoadedCalled;

    private static class FileProviderHelper implements ContentUriUtils.FileProviderUtil {
        // Keep this variable in sync with the value defined in AndroidManifest.xml.
        private static final String API_AUTHORITY_SUFFIX =
                ".org.chromium.weblayer.client.FileProvider";

        @Override
        public Uri getContentUriFromFile(File file) {
            Context appContext = ContextUtils.getApplicationContext();
            return FileProvider.getUriForFile(
                    appContext, appContext.getPackageName() + API_AUTHORITY_SUFFIX, file);
        }
    }

    WebLayerImpl() {}

    @Override
    public void loadAsync(IObjectWrapper appContextWrapper, IObjectWrapper remoteContextWrapper,
            IObjectWrapper loadedCallbackWrapper) {
        StrictModeWorkaround.apply();

        Context appContext = ObjectWrapper.unwrap(appContextWrapper, Context.class);
        Context remoteContext = ObjectWrapper.unwrap(remoteContextWrapper, Context.class);
        init(appContext, remoteContext);

        final ValueCallback<Boolean> loadedCallback = (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                loadedCallbackWrapper, ValueCallback.class);
        // WARNING: Ensure any method calls from this guard against the possibility of being called
        // multiple times (see comment in loadSync()).
        BrowserStartupController.getInstance().startBrowserProcessesAsync(
                LibraryProcessType.PROCESS_WEBLAYER,
                /* startGpu */ true, /* startMinimalBrowser */ false,
                new BrowserStartupController.StartupCallback() {
                    @Override
                    public void onSuccess() {
                        onNativeLoaded(appContext);
                        loadedCallback.onReceiveValue(true);
                    }
                    @Override
                    public void onFailure() {
                        loadedCallback.onReceiveValue(false);
                    }
                });
    }

    @Override
    public void loadSync(IObjectWrapper appContextWrapper, IObjectWrapper remoteContextWrapper) {
        StrictModeWorkaround.apply();

        Context appContext = ObjectWrapper.unwrap(appContextWrapper, Context.class);
        Context remoteContext = ObjectWrapper.unwrap(remoteContextWrapper, Context.class);
        init(appContext, remoteContext);

        BrowserStartupController.getInstance().startBrowserProcessesSync(
                LibraryProcessType.PROCESS_WEBLAYER,
                /*singleProcess=*/false, /*startGpuProcess=*/true);

        onNativeLoaded(appContext);
        // WARNING: loadAsync() may be in progress, and may call methods that this does as well.
        // Ensure any method calls from this guard against the possibility of being called multiple
        // times.
    }

    private void onNativeLoaded(Context appContext) {
        // This may be called multiple times, ensure processing only happens once.
        if (mOnNativeLoadedCalled) return;
        mOnNativeLoadedCalled = true;

        // TODO(swestphal): Move this earlier when it is not depending on native code being loaded.
        ChildProcessLauncherHelper.warmUp(appContext, true);

        CrashReporterController.getInstance().notifyNativeInitialized();
        NetworkChangeNotifier.init();
        NetworkChangeNotifier.registerToReceiveNotificationsAlways();

        // Native and variations has to be loaded before this.
        loadComponents();

        // This issues JNI calls which require native code to be loaded.
        MetricsServiceClient.init();

        WebLayerOriginVerificationScheduler.init(appContext.getPackageName(),
                mProfileManager.getProfile(/* name= */ "", true), appContext);

        assert mInited;
        WebLayerImplJni.get().setIsWebViewCompatMode(mIsWebViewCompatMode);
    }

    private void init(Context appContext, Context remoteContext) {
        if (mInited) {
            return;
        }
        mInited = true;

        UmaUtils.recordMainEntryPointTime();

        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_WEBLAYER);

        // The remote context will have a different class loader than WebLayerImpl here if we are in
        // WebView compat mode, since WebView compat mode creates it's own class loader. The class
        // loader from remoteContext will actually never be used, since
        // ClassLoaderContextWrapperFactory will override the class loader, and all contexts used in
        // WebLayer should come from ClassLoaderContextWrapperFactory.
        mIsWebViewCompatMode = remoteContext != null
                && !remoteContext.getClassLoader().equals(WebLayerImpl.class.getClassLoader());
        if (mIsWebViewCompatMode) {
            notifyWebViewRunningInProcess(remoteContext.getClassLoader());
        }

        Context wrappedAppContext = minimalInitForContext(appContext, remoteContext);

        GmsBridge.getInstance().checkClientAppContext(wrappedAppContext);

        // Load library in the background since it may be expensive.
        // TODO(crbug.com/1146438): Look into enabling relro sharing in browser process. It seems to
        // crash when WebView is loaded in the same process.
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                LibraryLoader.getInstance().loadNow();
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        PackageInfo packageInfo = WebViewFactory.getLoadedPackageInfo();

        if (!CommandLine.isInitialized()) {
            if (BuildInfo.isDebugAndroid()) {
                // This disk read in the critical path is for development purposes only.
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    File localCommandLineFile =
                            new File(wrappedAppContext.getFilesDir(), COMMAND_LINE_FILE);
                    if (localCommandLineFile.exists()) {
                        CommandLine.initFromFile(localCommandLineFile.getPath());
                    } else {
                        CommandLine.initFromFile(PUBLIC_COMMAND_LINE_FILE);
                    }
                }
            } else {
                CommandLine.init(null);
            }
        }

        TraceEvent.maybeEnableEarlyTracing(/*readCommandLine=*/true);
        TraceEvent.begin("WebLayer init");

        WebApkValidator.init(
                ChromeWebApkHostSignature.EXPECTED_SIGNATURE, ChromeWebApkHostSignature.PUBLIC_KEY);

        BuildInfo.setBrowserPackageInfo(packageInfo);
        BuildInfo.setFirebaseAppId(
                FirebaseConfig.getFirebaseAppIdForPackage(packageInfo.packageName));

        SelectionPopupController.setMustUseWebContentsContext();
        SelectionPopupController.setShouldGetReadbackViewFromWindowAndroid();

        ResourceBundle.setAvailablePakLocales(ProductConfig.LOCALES);
        BundleUtils.setIsBundle(ProductConfig.IS_BUNDLE);

        setChildProcessCreationParams(wrappedAppContext, packageInfo.packageName);

        // Creating the Android shared preferences object causes I/O.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
            deleteDataIfPackageDowngrade(prefs, packageInfo);
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());

        GmsBridge.getInstance().setSafeBrowsingHandler();
        GmsBridge.getInstance().initializeBuiltInPaymentApps();

        MediaStreamManager.onWebLayerInit();
        WebLayerNotificationChannels.updateChannelsIfNecessary();

        ContactsPicker.setContactsPickerDelegate(
                (WindowAndroid windowAndroid, ContactsPickerListener listener,
                        boolean allowMultiple, boolean includeNames, boolean includeEmails,
                        boolean includeTel, boolean includeAddresses, boolean includeIcons,
                        String formattedOrigin) -> {
                    ContactsPickerDialog dialog = new ContactsPickerDialog(windowAndroid,
                            new ContactsPickerAdapter(windowAndroid), listener, allowMultiple,
                            includeNames, includeEmails, includeTel, includeAddresses, includeIcons,
                            formattedOrigin);
                    dialog.show();
                    return dialog;
                });

        DecoderServiceHost.setIntentSupplier(() -> { return createImageDecoderServiceIntent(); });
        SelectFileDialog.setPhotoPickerDelegate(new PhotoPickerDelegateBase() {
            @Override
            public PhotoPicker showPhotoPicker(WindowAndroid windowAndroid,
                    PhotoPickerListener listener, boolean allowMultiple, List<String> mimeTypes) {
                PhotoPickerDialog dialog = new PhotoPickerDialog(windowAndroid,
                        windowAndroid.getContext().get().getContentResolver(), listener,
                        allowMultiple, mimeTypes);
                dialog.show();
                return dialog;
            }
        });

        Clipboard.getInstance().setImageFileProvider(new ClipboardImageFileProvider());

        // Clear previously shared images from disk in the background.
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                ShareImageFileUtils.clearSharedImages();
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        performDexFixIfNecessary(packageInfo);

        TraceEvent.end("WebLayer init");
    }

    @Override
    public IBrowser createBrowser(IObjectWrapper serviceContext, IObjectWrapper fragmentArgs) {
        StrictModeWorkaround.apply();
        Bundle unwrappedFragmentArgs = ObjectWrapper.unwrap(fragmentArgs, Bundle.class);
        Context unwrappedServiceContext = ObjectWrapper.unwrap(serviceContext, Context.class);
        BrowserImpl browser =
                new BrowserImpl(unwrappedServiceContext, mProfileManager, unwrappedFragmentArgs);
        return browser;
    }

    @Override
    public IProfile getProfile(String profileName) {
        StrictModeWorkaround.apply();
        boolean isIncognito = "".equals(profileName);
        return mProfileManager.getProfile(profileName, isIncognito);
    }

    @Override
    public IProfile getIncognitoProfile(String profileName) {
        StrictModeWorkaround.apply();
        return mProfileManager.getProfile(profileName, true);
    }

    @Override
    public void setRemoteDebuggingEnabled(boolean enabled) {
        StrictModeWorkaround.apply();
        WebLayerImplJni.get().setRemoteDebuggingEnabled(enabled);
    }

    @Override
    public boolean isRemoteDebuggingEnabled() {
        StrictModeWorkaround.apply();
        return WebLayerImplJni.get().isRemoteDebuggingEnabled();
    }

    @Override
    public void onReceivedBroadcast(IObjectWrapper appContextWrapper, Intent intent) {
        StrictModeWorkaround.apply();
        Context context = ObjectWrapper.unwrap(appContextWrapper, Context.class);

        if (IntentUtils.handleIntent(intent)) return;

        if (intent.getAction().startsWith(DownloadImpl.getIntentPrefix())) {
            DownloadImpl.forwardIntent(context, intent, mProfileManager);
        }
    }

    @Override
    public void onMediaSessionServiceStarted(IObjectWrapper sessionService, Intent intent) {
        StrictModeWorkaround.apply();
        MediaSessionManager.serviceStarted(
                ObjectWrapper.unwrap(sessionService, Service.class), intent);
    }

    @Override
    public void onMediaSessionServiceDestroyed() {
        StrictModeWorkaround.apply();
        MediaSessionManager.serviceDestroyed();
    }

    @Override
    public void onRemoteMediaServiceStarted(IObjectWrapper sessionService, Intent intent) {
        StrictModeWorkaround.apply();
        MediaRouterClientImpl.serviceStarted(
                ObjectWrapper.unwrap(sessionService, Service.class), intent);
    }

    @Override
    public void onRemoteMediaServiceDestroyed(int id) {
        StrictModeWorkaround.apply();
        MediaRouterClientImpl.serviceDestroyed(id);
    }

    @Override
    public IBinder initializeImageDecoder(IObjectWrapper appContext, IObjectWrapper remoteContext) {
        StrictModeWorkaround.apply();

        assert ContextUtils.getApplicationContext() == null;
        CommandLine.init(null);
        minimalInitForContext(ObjectWrapper.unwrap(appContext, Context.class),
                ObjectWrapper.unwrap(remoteContext, Context.class));
        LibraryLoader.getInstance().setLibraryProcessType(
                LibraryProcessType.PROCESS_WEBLAYER_CHILD);
        LibraryLoader.getInstance().ensureInitialized();

        ImageDecoder imageDecoder = new ImageDecoder();
        imageDecoder.initializeSandbox();
        return imageDecoder;
    }

    @Override
    public IObjectWrapper createGooglePayDataCallbacksService() {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(GmsBridge.getInstance().createGooglePayDataCallbacksService());
    }

    @Override
    public IObjectWrapper createPaymentDetailsUpdateService() {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(new PaymentDetailsUpdateService());
    }

    @Override
    public void enumerateAllProfileNames(IObjectWrapper valueCallback) {
        StrictModeWorkaround.apply();
        final ValueCallback<String[]> callback =
                (ValueCallback<String[]>) ObjectWrapper.unwrap(valueCallback, ValueCallback.class);
        ProfileImpl.enumerateAllProfileNames(callback);
    }

    @Override
    public void setClient(IWebLayerClient client) {
        StrictModeWorkaround.apply();
        sClient = client;

        if (WebLayerFactoryImpl.getClientMajorVersion() >= 88) {
            try {
                RecordHistogram.recordTimesHistogram("WebLayer.Startup.ClassLoaderCreationTime",
                        sClient.getClassLoaderCreationTime());
                RecordHistogram.recordTimesHistogram(
                        "WebLayer.Startup.ContextCreationTime", sClient.getContextCreationTime());
                RecordHistogram.recordTimesHistogram("WebLayer.Startup.WebLayerLoaderCreationTime",
                        sClient.getWebLayerLoaderCreationTime());
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
    }

    @Override
    public String getUserAgentString() {
        StrictModeWorkaround.apply();
        return WebLayerImplJni.get().getUserAgentString();
    }

    @Override
    public void registerExternalExperimentIDs(String trialName, int[] experimentIDs) {
        StrictModeWorkaround.apply();
        WebLayerImplJni.get().registerExternalExperimentIDs(experimentIDs);
    }

    @Override
    public String getXClientDataHeader() {
        StrictModeWorkaround.apply();
        return WebLayerImplJni.get().getXClientDataHeader();
    }

    @Override
    public IObjectWrapper getApplicationContext() {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(ContextUtils.getApplicationContext());
    }

    public static int getMaxNavigationsPerTabForInstanceState() {
        try {
            return (WebLayerFactoryImpl.getClientMajorVersion() >= 98)
                    ? sClient.getMaxNavigationsPerTabForInstanceState()
                    : 0;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static Intent createIntent() {
        if (sClient == null) {
            throw new IllegalStateException("WebLayer should have been initialized already.");
        }

        try {
            return sClient.createIntent();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static Intent createMediaSessionServiceIntent() {
        if (sClient == null) {
            throw new IllegalStateException("WebLayer should have been initialized already.");
        }

        try {
            return sClient.createMediaSessionServiceIntent();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static Intent createImageDecoderServiceIntent() {
        if (sClient == null) {
            throw new IllegalStateException("WebLayer should have been initialized already.");
        }

        try {
            return sClient.createImageDecoderServiceIntent();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static int getMediaSessionNotificationId() {
        if (sClient == null) {
            throw new IllegalStateException("WebLayer should have been initialized already.");
        }

        try {
            return sClient.getMediaSessionNotificationId();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static Intent createRemoteMediaServiceIntent() {
        if (sClient == null) {
            throw new IllegalStateException("WebLayer should have been initialized already.");
        }

        try {
            return sClient.createRemoteMediaServiceIntent();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static int getPresentationApiNotificationId() {
        if (sClient == null) {
            throw new IllegalStateException("WebLayer should have been initialized already.");
        }

        try {
            return sClient.getPresentationApiNotificationId();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static int getRemotePlaybackApiNotificationId() {
        if (sClient == null) {
            throw new IllegalStateException("WebLayer should have been initialized already.");
        }

        try {
            return sClient.getRemotePlaybackApiNotificationId();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static String getClientApplicationName() {
        Context context = ContextUtils.getApplicationContext();
        return new StringBuilder()
                .append(context.getPackageManager().getApplicationLabel(
                        context.getApplicationInfo()))
                .toString();
    }

    /**
     * Converts the given id into a resource ID that can be shown in system UI, such as
     * notifications.
     */
    public static int getResourceIdForSystemUi(int id) {
        if (isAndroidResource(id)) {
            return id;
        }

        Context context = ContextUtils.getApplicationContext();
        try {
            // String may be missing translations, since they are loaded at a different package ID
            // by default in standalone WebView.
            assert !context.getResources().getResourceTypeName(id).equals("string");
        } catch (Resources.NotFoundException e) {
        }
        id &= 0x00ffffff;
        id |= (0x01000000
                * getPackageId(context, WebViewFactory.getLoadedPackageInfo().packageName));
        return id;
    }

    /** Returns whether this ID is from the android system package. */
    public static boolean isAndroidResource(int id) {
        try {
            return ContextUtils.getApplicationContext()
                    .getResources()
                    .getResourcePackageName(id)
                    .equals("android");
        } catch (Resources.NotFoundException e) {
            return false;
        }
    }

    private static Context minimalInitForContext(Context appContext, Context remoteContext) {
        if (ContextUtils.getApplicationContext() != null) {
            return ContextUtils.getApplicationContext();
        }

        assert remoteContext != null;
        Context lightContext = createContextForMode(remoteContext, Configuration.UI_MODE_NIGHT_NO);
        Context darkContext = createContextForMode(remoteContext, Configuration.UI_MODE_NIGHT_YES);
        ClassLoaderContextWrapperFactory.setLightDarkResourceOverrideContext(
                lightContext, darkContext);

        int lightPackageId = forceCorrectPackageId(lightContext);
        int darkPackageId = forceCorrectPackageId(darkContext);
        assert lightPackageId == darkPackageId;

        // TODO: The call to onResourcesLoaded() can be slow, we may need to parallelize this with
        // other expensive startup tasks.
        R.onResourcesLoaded(lightPackageId);

        // Wrap the app context so that it can be used to load WebLayer implementation classes.
        appContext = ClassLoaderContextWrapperFactory.get(appContext);
        ContextUtils.initApplicationContext(appContext);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DIRECTORY_SUFFIX, PRIVATE_DIRECTORY_SUFFIX);
        return appContext;
    }

    /** Forces the correct package ID or dies with a runtime exception. */
    private static int forceCorrectPackageId(Context remoteContext) {
        int packageId = getPackageId(remoteContext, remoteContext.getPackageName());
        // This is using app_as_shared_lib, no change needed.
        if (packageId >= 0x7f) {
            return packageId;
        }

        if (packageId > REQUIRED_PACKAGE_IDENTIFIER) {
            throw new AndroidRuntimeException(
                    "WebLayer can't be used with other shared libraries. Package ID: " + packageId
                    + ", Loaded packages: " + getLoadedPackageNames(remoteContext));
        }

        forceAddAssetPaths(remoteContext, packageId);

        return REQUIRED_PACKAGE_IDENTIFIER;
    }

    /** Forces adding entries to the package identifiers array until we hit the required ID. */
    private static void forceAddAssetPaths(Context remoteContext, int packageId) {
        try {
            Method addAssetPath = AssetManager.class.getMethod("addAssetPath", String.class);
            String path = remoteContext.getApplicationInfo().sourceDir;
            // Add enough paths to make sure we reach the required ID.
            for (int i = packageId; i < REQUIRED_PACKAGE_IDENTIFIER; i++) {
                // Change the path to ensure the asset path is re-added and grabs a new package ID.
                path = "/." + path;
                addAssetPath.invoke(remoteContext.getAssets(), path);
            }
        } catch (ReflectiveOperationException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    /**
     * Returns the package ID to use when calling R.onResourcesLoaded().
     */
    private static int getPackageId(Context appContext, String implPackageName) {
        try {
            Constructor<WebViewDelegate> constructor =
                    WebViewDelegate.class.getDeclaredConstructor();
            constructor.setAccessible(true);
            WebViewDelegate delegate = constructor.newInstance();
            return delegate.getPackageId(appContext.getResources(), implPackageName);
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    /** Gets a string with all the loaded package names in this context. */
    private static String getLoadedPackageNames(Context appContext) {
        try {
            Method getAssignedPackageIdentifiers =
                    AssetManager.class.getMethod("getAssignedPackageIdentifiers");
            SparseArray<String> packageIdentifiers =
                    (SparseArray) getAssignedPackageIdentifiers.invoke(
                            appContext.getResources().getAssets());
            List<String> packageNames = new ArrayList<>();
            for (int i = 0; i < packageIdentifiers.size(); i++) {
                String name = packageIdentifiers.valueAt(i);
                int key = packageIdentifiers.keyAt(i);
                // This is the android package.
                if (key == 1) {
                    continue;
                }

                // Make sure this doesn't look like a URL so it doesn't get removed from crashes.
                packageNames.add(name.replace(".", "_") + " -> " + key);
            }
            return TextUtils.join(",", packageNames);
        } catch (ReflectiveOperationException e) {
            return "unknown";
        }
    }

    private void setChildProcessCreationParams(Context appContext, String implPackageName) {
        final boolean bindToCaller = true;
        final boolean ignoreVisibilityForImportance = false;
        final String privilegedServicesPackageName = appContext.getPackageName();
        final String privilegedServicesName =
                "org.chromium.weblayer.ChildProcessService$Privileged";

        String sandboxedServicesPackageName = appContext.getPackageName();
        String sandboxedServicesName = "org.chromium.weblayer.ChildProcessService$Sandboxed";
        boolean isExternalService = false;
        boolean loadedFromWebView = wasLoadedFromWebView(appContext);
        if (loadedFromWebView && supportsBindingToWebViewService(appContext, implPackageName)) {
            // When loading from a WebView implementation, use WebView's declared external services
            // as our renderers. This means on O+ we benefit from the webview zygote process, and on
            // other versions we ensure the client app doesn't slow down isolated process startup.
            // We still need to use the client's privileged services, as only isolated services can
            // be external.
            isExternalService = true;
            sandboxedServicesPackageName = implPackageName;
            sandboxedServicesName = null;
        }

        ChildProcessCreationParams.set(privilegedServicesPackageName, privilegedServicesName,
                sandboxedServicesPackageName, sandboxedServicesName, isExternalService,
                LibraryProcessType.PROCESS_WEBLAYER_CHILD, bindToCaller,
                ignoreVisibilityForImportance);
    }

    private static boolean supportsBindingToWebViewService(Context context, String packageName) {
        // Android N has issues with WebView with the non-system user.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            try {
                PackageInfo packageInfo =
                        context.getPackageManager().getPackageInfo(packageName, 0);
                // Package may be disabled for non-system users.
                if (!packageInfo.applicationInfo.enabled) {
                    return false;
                }
            } catch (PackageManager.NameNotFoundException e) {
                // Package may be uninstalled for non-system users.
                return false;
            }
        }
        return true;
    }

    private static boolean wasLoadedFromWebView(Context appContext) {
        try {
            Bundle metaData = appContext.getPackageManager()
                                      .getApplicationInfo(appContext.getPackageName(),
                                              PackageManager.GET_META_DATA)
                                      .metaData;
            if (metaData != null && metaData.getString(PACKAGE_MANIFEST_KEY) != null) {
                return false;
            }
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            // This would indicate the client app doesn't exist;
            // just return true as there's nothing sensible to do here.
            return true;
        }
    }

    private static void deleteDataIfPackageDowngrade(
            SharedPreferences prefs, PackageInfo packageInfo) {
        int previousVersion = prefs.getInt(PREF_LAST_VERSION_CODE, 0);
        int currentVersion = packageInfo.versionCode;
        if (getBranchFromVersionCode(currentVersion) < getBranchFromVersionCode(previousVersion)) {
            // WebLayer was downgraded since the last run. Delete the data and cache directories.
            File dataDir = new File(PathUtils.getDataDirectory());
            Log.i(TAG,
                    "WebLayer package downgraded from " + previousVersion + " to " + currentVersion
                            + "; deleting contents of " + dataDir);
            deleteDirectoryContents(dataDir);
        }
        if (previousVersion != currentVersion) {
            prefs.edit().putInt(PREF_LAST_VERSION_CODE, currentVersion).apply();
        }
    }

    /**
     * Chromium versionCodes follow the scheme "BBBBPPPAX":
     * BBBB: 4 digit branch number. It monotonically increases over time.
     * PPP:  Patch number in the branch. It is padded with zeroes to the left. These three digits
     *       may change their meaning in the future.
     * A:    Architecture digit.
     * X:    A digit to differentiate APKs for other reasons.
     *
     * @return The branch number of versionCode.
     */
    private static int getBranchFromVersionCode(int versionCode) {
        return versionCode / 1_000_00;
    }

    private static void deleteDirectoryContents(File directory) {
        File[] files = directory.listFiles();
        if (files == null) {
            return;
        }
        for (File file : files) {
            if (!FileUtils.recursivelyDeleteFile(file, FileUtils.DELETE_ALL)) {
                Log.w(TAG, "Failed to delete " + file);
            }
        }
    }

    private static void notifyWebViewRunningInProcess(ClassLoader webViewClassLoader) {
        // TODO(crbug.com/1112001): Investigate why loading classes causes strict mode
        // violations in some situations.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            Class<?> webViewChromiumFactoryProviderClass =
                    Class.forName("com.android.webview.chromium.WebViewChromiumFactoryProvider",
                            true, webViewClassLoader);
            Method setter = webViewChromiumFactoryProviderClass.getDeclaredMethod(
                    "setWebLayerRunningInSameProcess");
            setter.invoke(null);
        } catch (Exception e) {
            Log.w(TAG, "Unable to notify WebView running in process.");
        }
    }

    @SuppressLint("DiscouragedPrivateApi")
    private static Context createContextForMode(Context remoteContext, int uiMode) {
        Configuration configuration = new Configuration();
        configuration.uiMode = uiMode;
        Context context = remoteContext.createConfigurationContext(configuration);
        // DrawableInflater uses the ClassLoader from the Resources object. We need to make sure
        // this ClassLoader is correct. See crbug.com/1287000 and crbug.com/1293849 for more
        // details.
        try {
            Field classLoaderField = Resources.class.getDeclaredField("mClassLoader");
            classLoaderField.setAccessible(true);
            classLoaderField.set(context.getResources(), WebLayerImpl.class.getClassLoader());
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Error setting Resources ClassLoader.", e);
        }
        return context;
    }

    @CalledByNative
    @Nullable
    private static String getEmbedderName() {
        return getClientApplicationName();
    }

    /*
     * Android O MR1 has a bug where bg-dexopt-job will break optimized dex files for isolated
     * splits. This leads to *very* slow startup on those devices. To mitigate this, we attempt
     * to force a dex compile if necessary.
     */
    private static void performDexFixIfNecessary(PackageInfo packageInfo) {
        if (Build.VERSION.SDK_INT != Build.VERSION_CODES.O_MR1) {
            return;
        }

        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
            ApplicationInfo appInfo = packageInfo.applicationInfo;
            String[] splitNames = appInfo.splitNames;
            if (splitNames == null) {
                return;
            }

            for (int i = 0; i < splitNames.length; i++) {
                String splitName = splitNames[i];
                // WebLayer depends on the "weblayer" split and "chrome" split (if running in
                // Monochrome).
                if (!splitName.equals("chrome") && !splitName.equals("weblayer")) {
                    continue;
                }
                String splitDir = appInfo.splitSourceDirs[i];
                try {
                    if (DexFile.isDexOptNeeded(splitDir)) {
                        String cmd = String.format("cmd package compile -r shared --split %s %s",
                                new File(splitDir).getName(), packageInfo.packageName);
                        Runtime.getRuntime().exec(cmd);
                    }
                } catch (IOException e) {
                    Log.e(TAG, "Error fixing dex files.", e);
                }
            }
        });
    }

    /**
     * Load components files from {@link
     * org.chromium.android_webview.services.ComponentsProviderService}.
     */
    private static void loadComponents() {
        ComponentLoaderPolicyBridge[] componentPolicies =
                WebLayerImplJni.get().getComponentLoaderPolicies();
        // Don't connect to the service if there are no components to load.
        if (componentPolicies.length == 0) {
            return;
        }
        final Intent intent = new Intent();
        intent.setClassName(getWebViewFactoryPackageName(),
                EmbeddedComponentLoader.AW_COMPONENTS_PROVIDER_SERVICE);
        new EmbeddedComponentLoader(Arrays.asList(componentPolicies)).connect(intent);
    }

    /**
     * WebViewFactory is not a public android API so R8 is unable to compute its
     * API level. This causes R8 to not be able to inline WebLayerImplJni#get
     * into WebLayerImpl#loadComponents.

     * References to WebViewFactory are in a separate method to avoid this issue
     * and allow WebLayerImplJni#get to be inlined into WebLayerImpl#loadComponents.
     * @DoNotInline is to avoid any similar inlining issues whenever this method
     * is referenced.
     */
    @DoNotInline
    private static String getWebViewFactoryPackageName() {
        return WebViewFactory.getLoadedPackageInfo().packageName;
    }

    @NativeMethods
    interface Natives {
        void setRemoteDebuggingEnabled(boolean enabled);
        boolean isRemoteDebuggingEnabled();
        void setIsWebViewCompatMode(boolean value);
        String getUserAgentString();
        void registerExternalExperimentIDs(int[] experimentIDs);
        String getXClientDataHeader();
        ComponentLoaderPolicyBridge[] getComponentLoaderPolicies();
    }
}
