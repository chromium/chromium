// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.support.v4.content.FileProvider;
import android.util.SparseArray;
import android.webkit.ValueCallback;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.ui.base.ResourceBundle;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.WebLayerVersion;

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/**
 * Root implementation class for WebLayer.
 * This is constructed by the client library using reflection.
 */
@JNINamespace("weblayer")
@UsedByReflection("WebLayer")
public final class WebLayerImpl extends IWebLayer.Stub {
    // TODO: should there be one tag for all this code?
    private static final String TAG = "WebLayer";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "weblayer";
    // TODO: Configure this from the client.
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/weblayer-command-line";

    private final ProfileManager mProfileManager = new ProfileManager();

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

    @UsedByReflection("WebLayer")
    public static IBinder create() {
        return new WebLayerImpl();
    }

    private WebLayerImpl() {}

    /**
     * Returns true if the client and implementation versions are compatible.
     */
    @UsedByReflection("WebLayer")
    public static boolean checkVersion(int clientVersion) {
        return clientVersion == WebLayerVersion.sVersionNumber;
    }

    @Override
    public void initAndLoadAsync(
            IObjectWrapper appContextWrapper, IObjectWrapper loadedCallbackWrapper) {
        // Wrap the app context so that it can be used to load WebLayer implementation classes.
        Context appContext = ClassLoaderContextWrapperFactory.get(
                ObjectWrapper.unwrap(appContextWrapper, Context.class));
        PackageInfo packageInfo = WebViewFactory.getLoadedPackageInfo();
        ContextUtils.initApplicationContext(appContext);
        BuildInfo.setBrowserPackageInfo(packageInfo);
        int resourcesPackageId = getPackageId(appContext, packageInfo.packageName);
        // TODO: The call to onResourcesLoaded() can be slow, we may need to parallelize this with
        // other expensive startup tasks.
        R.onResourcesLoaded(resourcesPackageId);

        ResourceBundle.setAvailablePakLocales(new String[] {}, ProductConfig.UNCOMPRESSED_LOCALES);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);

        ChildProcessCreationParams.set(appContext.getPackageName(), false /* isExternalService */,
                LibraryProcessType.PROCESS_WEBLAYER_CHILD, true /* bindToCaller */,
                false /* ignoreVisibilityForImportance */,
                "org.chromium.weblayer.ChildProcessService$Privileged",
                "org.chromium.weblayer.ChildProcessService$Sandboxed");

        if (!CommandLine.isInitialized()) {
            // This disk read in the critical path is for development purposes only.
            // TODO: Move it to debug-only (similar to WebView), or allow clients to configure the
            // command line.
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                CommandLine.initFromFile(COMMAND_LINE_FILE);
            }
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());

        // TODO: Validate that doing this disk IO on the main thread is necessary.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_WEBLAYER);
        }
        GmsBridge.getInstance().setSafeBrowsingHandler();

        final ValueCallback<Boolean> loadedCallback = (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                loadedCallbackWrapper, ValueCallback.class);
        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesAsync(/* startGpu */ false,
                        /* startServiceManagerOnly */ false,
                        new BrowserStartupController.StartupCallback() {
                            @Override
                            public void onSuccess() {
                                loadedCallback.onReceiveValue(true);
                            }
                            @Override
                            public void onFailure() {
                                loadedCallback.onReceiveValue(false);
                            }
                        });
    }

    @Override
    public void loadSync() {
        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesSync(
                        /* singleProcess*/ false);
    }

    @Override
    public IBrowserFragment createBrowserFragmentImpl(
            IRemoteFragmentClient fragmentClient, IObjectWrapper fragmentArgs) {
        Bundle unwrappedArgs = ObjectWrapper.unwrap(fragmentArgs, Bundle.class);
        BrowserFragmentImpl fragment =
                new BrowserFragmentImpl(mProfileManager, fragmentClient, unwrappedArgs);
        return fragment.asIBrowserFragment();
    }

    @Override
    public IProfile getProfile(String profilePath) {
        return mProfileManager.getProfile(profilePath);
    }

    @Override
    public void setRemoteDebuggingEnabled(boolean enabled) {
        WebLayerImplJni.get().setRemoteDebuggingEnabled(enabled);
    }

    @Override
    public boolean isRemoteDebuggingEnabled() {
        return WebLayerImplJni.get().isRemoteDebuggingEnabled();
    }

    /**
     * Returns the package ID to use when calling R.onResourcesLoaded().
     */
    private static int getPackageId(Context appContext, String implPackageName) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
                Constructor constructor = WebViewDelegate.class.getDeclaredConstructor();
                constructor.setAccessible(true);
                WebViewDelegate delegate = (WebViewDelegate) constructor.newInstance();
                return delegate.getPackageId(appContext.getResources(), implPackageName);
            } else {
                // In L WebViewDelegate did not yet exist, so we have to look inside AssetManager.
                Method getAssignedPackageIdentifiers =
                        AssetManager.class.getMethod("getAssignedPackageIdentifiers");
                SparseArray packageIdentifiers = (SparseArray) getAssignedPackageIdentifiers.invoke(
                        appContext.getResources().getAssets());
                for (int i = 0; i < packageIdentifiers.size(); i++) {
                    final String name = (String) packageIdentifiers.valueAt(i);

                    if (implPackageName.equals(name)) {
                        return packageIdentifiers.keyAt(i);
                    }
                }
                throw new RuntimeException("Package not found: " + implPackageName);
            }
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    @NativeMethods
    interface Natives {
        void setRemoteDebuggingEnabled(boolean enabled);
        boolean isRemoteDebuggingEnabled();
    }
}
