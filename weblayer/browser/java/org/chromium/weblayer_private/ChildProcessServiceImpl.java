// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.webkit.WebViewFactory;

import com.google.android.gms.common.GooglePlayServicesUtilLight;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.NativeLibraryPreloader;
import org.chromium.base.process_launcher.ChildProcessService;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.app.ChildProcessServiceFactory;
import org.chromium.weblayer_private.interfaces.IChildProcessService;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.lang.reflect.Method;

/**
 * Implementation of IChildProcessService.
 */
@UsedByReflection("WebLayer")
public final class ChildProcessServiceImpl extends IChildProcessService.Stub {
    private static final String TAG = "WebLayer";
    private ChildProcessService mService;

    @UsedByReflection("WebLayer")
    public static IBinder create(Service service, Context appContext, Context remoteContext) {
        setLibraryPreloader(remoteContext.getPackageName(), remoteContext.getClassLoader());
        ClassLoaderContextWrapperFactory.setLightDarkResourceOverrideContext(
                remoteContext, remoteContext);
        // Wrap the app context so that it can be used to load WebLayer implementation classes.
        appContext = ClassLoaderContextWrapperFactory.get(appContext);

        // The GPU process may use GMS APIs using the host app's context. This is called in the
        // browser process in GmsBridge.
        GooglePlayServicesUtilLight.enableUsingApkIndependentContext();

        return new ChildProcessServiceImpl(service, appContext);
    }

    @Override
    public void onCreate() {
        StrictModeWorkaround.apply();
        mService.onCreate();
    }

    @Override
    public void onDestroy() {
        StrictModeWorkaround.apply();
        mService.onDestroy();
        mService = null;
    }

    @Override
    public IObjectWrapper onBind(IObjectWrapper intent) {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(mService.onBind(ObjectWrapper.unwrap(intent, Intent.class)));
    }

    private ChildProcessServiceImpl(Service service, Context context) {
        mService = ChildProcessServiceFactory.create(service, context);
    }

    public static void setLibraryPreloader(String webLayerPackageName, ClassLoader classLoader) {
        if (!LibraryLoader.getInstance().isLoadedByZygote()) {
            LibraryLoader.getInstance().setNativeLibraryPreloader(new NativeLibraryPreloader() {
                @Override
                public int loadLibrary(String packageName) {
                    return loadNativeLibrary(webLayerPackageName, classLoader);
                }
            });
        }
    }

    private static int loadNativeLibrary(String packageName, ClassLoader cl) {
        // Loading the library triggers disk access.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                return WebViewFactory.loadWebViewNativeLibraryFromPackage(packageName, cl);
            } else {
                try {
                    Method loadNativeLibrary =
                            WebViewFactory.class.getDeclaredMethod("loadNativeLibrary");
                    loadNativeLibrary.setAccessible(true);
                    loadNativeLibrary.invoke(null);
                    return 0; // LIBLOAD_SUCCESS
                } catch (ReflectiveOperationException e) {
                    Log.e(TAG, "Failed to load native library.", e);
                    return 6; // LIBLOAD_FAILED_TO_LOAD_LIBRARY
                }
            }
        }
    }
}
