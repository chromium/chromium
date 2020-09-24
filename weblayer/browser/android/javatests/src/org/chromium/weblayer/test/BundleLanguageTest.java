// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.SparseArray;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Locale;

/** Tests that translations work correctly for Java strings inside bundles. */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class BundleLanguageTest {
    private static final String WEBLAYER_SPECIFIC_STRING = "string/geolocation_permission_title";
    private static final String SHARED_STRING = "string/color_picker_dialog_title";

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private Context mRemoteContext;
    private Context mWebLayerContext;

    @Before
    public void setUp() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        mRemoteContext = TestWebLayer.getRemoteContext(activity.getApplicationContext());
        mWebLayerContext = TestWebLayer.getWebLayerContext(activity.getApplicationContext());
    }

    @Test
    @SmallTest
    public void testWebLayerString() throws Exception {
        // The bundle tests have both "es" and "fr" splits installed, so each of these should have a
        // separate translation.
        HashSet<String> translations = new HashSet<>();
        for (String locale : new String[] {"en", "es", "fr"}) {
            translations.add(getStringForLocale(WEBLAYER_SPECIFIC_STRING, locale));
        }
        Assert.assertEquals(3, translations.size());

        // The "ko" language split is not installed, so should fall back to english.
        Assert.assertEquals(getStringForLocale(WEBLAYER_SPECIFIC_STRING, "en"),
                getStringForLocale(WEBLAYER_SPECIFIC_STRING, "ko"));
    }

    @Test
    @SmallTest
    public void testSharedString() throws Exception {
        // This string is shared with WebView, so should have a separate translation for all
        // locales, even locales without splits installed.
        HashSet<String> translations = new HashSet<>();
        for (String locale : new String[] {"en", "es", "fr", "ko"}) {
            translations.add(getStringForLocale(SHARED_STRING, locale));
        }
        Assert.assertEquals(4, translations.size());
    }

    /**
     * Tests that all locale resources have been moved into splits, so the only package ID left in
     * the base APK has a dynamic ID.
     */
    @Test
    @SmallTest
    public void testBasePackageIdCorrect() throws Exception {
        AssetManager assetManager = createEmptyAssetManager();
        addAssetPath(assetManager, mWebLayerContext.getApplicationInfo().sourceDir);
        SparseArray<String> packageIds = getPackageIds(assetManager);
        Assert.assertEquals(2, packageIds.size());
        Assert.assertEquals(packageIds.get(1), "android");
        Assert.assertEquals(packageIds.get(2), mWebLayerContext.getPackageName());
    }

    /** Tests that locale splits only have resources from the hardcoded locale package ID. */
    @Test
    @SmallTest
    public void testLocalePackageIdCorrect() throws Exception {
        AssetManager assetManager = createEmptyAssetManager();
        for (String path : mWebLayerContext.getApplicationInfo().splitSourceDirs) {
            addAssetPath(assetManager, path);
        }
        SparseArray<String> packageIds = getPackageIds(assetManager);
        Assert.assertEquals(2, packageIds.size());
        Assert.assertEquals(packageIds.get(1), "android");
        Assert.assertEquals(packageIds.get(ResourceUtil.REQUIRED_PACKAGE_IDENTIFIER),
                mWebLayerContext.getPackageName() + "_translations");
    }

    private String getStringForLocale(String name, String locale) {
        Resources resources = mRemoteContext.getResources();
        Configuration config = resources.getConfiguration();
        config.setLocale(new Locale(locale));
        resources.updateConfiguration(config, resources.getDisplayMetrics());
        return resources.getString(ResourceUtil.getIdentifier(
                mRemoteContext, name, mWebLayerContext.getPackageName()));
    }

    private static AssetManager createEmptyAssetManager() throws ReflectiveOperationException {
        Constructor<AssetManager> constructor = AssetManager.class.getDeclaredConstructor();
        constructor.setAccessible(true);
        return constructor.newInstance();
    }

    private static void addAssetPath(AssetManager assetManager, String path)
            throws ReflectiveOperationException {
        Method addAssetPath = AssetManager.class.getMethod("addAssetPath", String.class);
        addAssetPath.invoke(assetManager, path);
    }

    private static SparseArray<String> getPackageIds(AssetManager assetManager)
            throws ReflectiveOperationException {
        Method getAssignedPackageIdentifiers =
                AssetManager.class.getMethod("getAssignedPackageIdentifiers");
        return (SparseArray) getAssignedPackageIdentifiers.invoke(assetManager);
    }
}
