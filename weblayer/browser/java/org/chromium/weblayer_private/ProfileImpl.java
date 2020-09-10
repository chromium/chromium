// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Intent;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.weblayer_private.interfaces.BrowsingDataType;
import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.SettingType;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Implementation of IProfile.
 */
@JNINamespace("weblayer")
public final class ProfileImpl extends IProfile.Stub implements BrowserContextHandle {
    private final String mName;
    private long mNativeProfile;
    private CookieManagerImpl mCookieManager;
    private Runnable mOnDestroyCallback;
    private boolean mBeingDeleted;
    private boolean mDownloadsInitialized;
    private DownloadCallbackProxy mDownloadCallbackProxy;
    private List<Intent> mDownloadNotificationIntents = new ArrayList<>();

    public static void enumerateAllProfileNames(ValueCallback<String[]> callback) {
        final Callback<String[]> baseCallback = (String[] names) -> callback.onReceiveValue(names);
        ProfileImplJni.get().enumerateAllProfileNames(baseCallback);
    }

    ProfileImpl(String name, Runnable onDestroyCallback) {
        if (!name.matches("^\\w*$")) {
            throw new IllegalArgumentException("Name can only contain words: " + name);
        }
        mName = name;
        mNativeProfile = ProfileImplJni.get().createProfile(name, ProfileImpl.this);
        mCookieManager =
                new CookieManagerImpl(ProfileImplJni.get().getCookieManager(mNativeProfile));
        mOnDestroyCallback = onDestroyCallback;
        mDownloadCallbackProxy = new DownloadCallbackProxy(mName, mNativeProfile);
    }

    private void destroyDependentJavaObjects() {
        if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }

        if (mCookieManager != null) {
            mCookieManager.destroy();
            mCookieManager = null;
        }
    }

    @Override
    public void destroy() {
        StrictModeWorkaround.apply();
        if (mBeingDeleted) return;

        destroyDependentJavaObjects();
        deleteNativeProfile();
        maybeRunDestroyCallback();
    }

    private void deleteNativeProfile() {
        ProfileImplJni.get().deleteProfile(mNativeProfile);
        mNativeProfile = 0;
    }

    private void maybeRunDestroyCallback() {
        if (mOnDestroyCallback == null) return;
        mOnDestroyCallback.run();
        mOnDestroyCallback = null;
    }

    @Override
    public void destroyAndDeleteDataFromDisk(IObjectWrapper completionCallback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        assert mNativeProfile != 0;
        if (ProfileImplJni.get().getNumBrowserImpl(mNativeProfile) > 0) {
            throw new IllegalStateException("Profile still in use: " + mName);
        }

        final Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);

        mBeingDeleted = true;
        destroyDependentJavaObjects();
        ProfileImplJni.get().destroyAndDeleteDataFromDisk(mNativeProfile, () -> {
            if (callback != null) callback.run();
        });
        mNativeProfile = 0;
        maybeRunDestroyCallback();
    }

    @Override
    public String getName() {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        return mName;
    }

    @Override
    public long getNativeBrowserContextPointer() {
        if (mNativeProfile == 0) {
            return 0;
        }
        return ProfileImplJni.get().getBrowserContext(mNativeProfile);
    }

    public boolean isIncognito() {
        return mName.isEmpty();
    }

    public boolean areDownloadsInitialized() {
        return mDownloadsInitialized;
    }

    public void addDownloadNotificationIntent(Intent intent) {
        mDownloadNotificationIntents.add(intent);
        ProfileImplJni.get().ensureBrowserContextInitialized(mNativeProfile);
    }

    @Override
    public void clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes, long fromMillis,
            long toMillis, @NonNull IObjectWrapper completionCallback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);
        ProfileImplJni.get().clearBrowsingData(
                mNativeProfile, mapBrowsingDataTypes(dataTypes), fromMillis, toMillis, callback);
    }

    @Override
    public void setDownloadDirectory(String directory) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        ProfileImplJni.get().setDownloadDirectory(mNativeProfile, directory);
    }

    @Override
    public void setDownloadCallbackClient(IDownloadCallbackClient client) {
        mDownloadCallbackProxy.setClient(client);
    }

    @Override
    public ICookieManager getCookieManager() {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        return mCookieManager;
    }

    @Override
    public void getBrowserPersistenceIds(@NonNull IObjectWrapper callback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        ValueCallback<Set<String>> valueCallback =
                (ValueCallback<Set<String>>) ObjectWrapper.unwrap(callback, ValueCallback.class);
        Callback<String[]> baseCallback = (String[] result) -> {
            valueCallback.onReceiveValue(new HashSet<String>(Arrays.asList(result)));
        };
        ProfileImplJni.get().getBrowserPersistenceIds(mNativeProfile, baseCallback);
    }

    @Override
    public void removeBrowserPersistenceStorage(String[] ids, @NonNull IObjectWrapper callback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        ValueCallback<Boolean> valueCallback =
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(callback, ValueCallback.class);
        Callback<Boolean> baseCallback = valueCallback::onReceiveValue;
        for (String id : ids) {
            if (TextUtils.isEmpty(id)) {
                throw new IllegalArgumentException("id must be non-null and non-empty");
            }
        }
        ProfileImplJni.get().removeBrowserPersistenceStorage(mNativeProfile, ids, baseCallback);
    }

    @Override
    public void prepareForPossibleCrossOriginNavigation() {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        ProfileImplJni.get().prepareForPossibleCrossOriginNavigation(mNativeProfile);
    }

    @Override
    public void getCachedFaviconForPageUri(@NonNull String uri, @NonNull IObjectWrapper callback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        ValueCallback<Bitmap> valueCallback =
                (ValueCallback<Bitmap>) ObjectWrapper.unwrap(callback, ValueCallback.class);
        Callback<Bitmap> baseCallback = valueCallback::onReceiveValue;
        ProfileImplJni.get().getCachedFaviconForPageUrl(mNativeProfile, uri, baseCallback);
    }

    void checkNotDestroyed() {
        if (!mBeingDeleted) return;
        throw new IllegalArgumentException("Profile being destroyed: " + mName);
    }

    private static @ImplBrowsingDataType int[] mapBrowsingDataTypes(
            @NonNull @BrowsingDataType int[] dataTypes) {
        // Convert data types coming from aidl to the ones accepted by C++ (ImplBrowsingDataType is
        // generated from a C++ enum).
        List<Integer> convertedTypes = new ArrayList<>();
        for (int aidlType : dataTypes) {
            switch (aidlType) {
                case BrowsingDataType.COOKIES_AND_SITE_DATA:
                    convertedTypes.add(ImplBrowsingDataType.COOKIES_AND_SITE_DATA);
                    break;
                case BrowsingDataType.CACHE:
                    convertedTypes.add(ImplBrowsingDataType.CACHE);
                    break;
                case BrowsingDataType.SITE_SETTINGS:
                    convertedTypes.add(ImplBrowsingDataType.SITE_SETTINGS);
                    break;
                default:
                    break; // Skip unrecognized values for forward compatibility.
            }
        }
        return CollectionUtil.integerListToIntArray(convertedTypes);
    }

    long getNativeProfile() {
        return mNativeProfile;
    }

    @CalledByNative
    public void downloadsInitialized() {
        mDownloadsInitialized = true;

        for (Intent intent : mDownloadNotificationIntents) {
            DownloadImpl.handleIntent(intent);
        }
        mDownloadNotificationIntents.clear();
    }

    @Override
    public void setBooleanSetting(@SettingType int type, boolean value) {
        ProfileImplJni.get().setBooleanSetting(mNativeProfile, type, value);
    }

    @Override
    public boolean getBooleanSetting(@SettingType int type) {
        return ProfileImplJni.get().getBooleanSetting(mNativeProfile, type);
    }

    @NativeMethods
    interface Natives {
        void enumerateAllProfileNames(Callback<String[]> callback);
        long createProfile(String name, ProfileImpl caller);
        void deleteProfile(long profile);
        long getBrowserContext(long nativeProfileImpl);
        int getNumBrowserImpl(long nativeProfileImpl);
        void destroyAndDeleteDataFromDisk(long nativeProfileImpl, Runnable completionCallback);
        void clearBrowsingData(long nativeProfileImpl, @ImplBrowsingDataType int[] dataTypes,
                long fromMillis, long toMillis, Runnable callback);
        void setDownloadDirectory(long nativeProfileImpl, String directory);
        long getCookieManager(long nativeProfileImpl);
        void ensureBrowserContextInitialized(long nativeProfileImpl);
        void setBooleanSetting(long nativeProfileImpl, int type, boolean value);
        boolean getBooleanSetting(long nativeProfileImpl, int type);
        void getBrowserPersistenceIds(long nativeProfileImpl, Callback<String[]> callback);
        void removeBrowserPersistenceStorage(
                long nativeProfileImpl, String[] ids, Callback<Boolean> callback);
        void prepareForPossibleCrossOriginNavigation(long nativeProfileImpl);
        void getCachedFaviconForPageUrl(
                long nativeProfileImpl, String url, Callback<Bitmap> callback);
    }
}
