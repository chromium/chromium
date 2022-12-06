// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.StrictMode;
import android.text.TextUtils;

import dalvik.system.BaseDexClassLoader;
import dalvik.system.PathClassLoader;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Helper class which performs initialization needed for WebView compatibility. */
final class WebViewCompatibilityHelper {
    /** Creates a the ClassLoader to use for WebView compatibility. */
    static ClassLoader initialize(Context appContext)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        Context remoteContext = WebLayer.getOrCreateRemoteContext(appContext);
        PackageInfo info =
                appContext.getPackageManager().getPackageInfo(remoteContext.getPackageName(),
                        PackageManager.GET_SHARED_LIBRARY_FILES
                                | PackageManager.MATCH_UNINSTALLED_PACKAGES);
        String[] libraryPaths = getLibraryPaths(remoteContext.getClassLoader());
        // Prepend "/." to all library paths. This changes the library path while still pointing to
        // the same directory, allowing us to get around a check in the JVM. This is only necessary
        // for N+, where we rely on linker namespaces.
        for (int i = 0; i < libraryPaths.length; i++) {
            assert libraryPaths[i].startsWith("/");
            libraryPaths[i] = "/." + libraryPaths[i];
        }

        String dexPath = getAllApkPaths(info.applicationInfo);
        String librarySearchPath = TextUtils.join(File.pathSeparator, libraryPaths);
        // TODO(cduvall): PathClassLoader may call stat on the library paths, consider moving
        // this to a background thread.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            // Use the system class loader's parent here, since it is much more efficient. This
            // matches what Android does when constructing class loaders, see
            // android.app.ApplicationLoaders.
            return new PathClassLoader(
                    dexPath, librarySearchPath, ClassLoader.getSystemClassLoader().getParent());
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /** Returns the library paths the given class loader will search. */
    static String[] getLibraryPaths(ClassLoader classLoader) throws ReflectiveOperationException {
        // This seems to be the best way to reliably get both the native lib directory and the path
        // within the APK where libs might be stored.
        return ((String) BaseDexClassLoader.class.getDeclaredMethod("getLdLibraryPath")
                        .invoke((BaseDexClassLoader) classLoader))
                .split(":");
    }

    /** This is mostly taken from ApplicationInfo.getAllApkPaths(). */
    private static String getAllApkPaths(ApplicationInfo info) {
        // The OS version of this method also includes resourceDirs, but this is not available in
        // the SDK.
        final List<String> output = new ArrayList<>(10);
        // First add the base APK path, since this is always needed.
        if (info.sourceDir != null) {
            output.add(info.sourceDir);
        }
        // Next add split paths that are used by WebLayer.
        if (info.splitSourceDirs != null) {
            String[] splitNames = null;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                splitNames = WebLayer.ApiHelperForO.getSplitNames(info);
            }
            for (int i = 0; i < info.splitSourceDirs.length; i++) {
                // WebLayer only uses the "chrome" and "weblayer" splits.
                if (splitNames != null && !splitNames[i].equals("chrome")
                        && !splitNames[i].equals("weblayer")) {
                    continue;
                }
                output.add(info.splitSourceDirs[i]);
            }
        }
        // Last, add shared library paths.
        if (info.sharedLibraryFiles != null) {
            for (String input : info.sharedLibraryFiles) {
                output.add(input);
            }
        }
        return TextUtils.join(File.pathSeparator, output);
    }

    private WebViewCompatibilityHelper() {}
}
