// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Intent;
import android.graphics.Bitmap;
import android.os.RemoteException;
import android.text.TextUtils;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.components.content_capture.PlatformContentCaptureController;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.BrowsingDataType;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IGoogleAccountAccessTokenFetcherClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IOpenUrlCallbackClient;
import org.chromium.weblayer_private.interfaces.IPrerenderController;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IProfileClient;
import org.chromium.weblayer_private.interfaces.IUserIdentityCallbackClient;
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
public final class ProfileImpl
        extends IProfile.Stub implements BrowserContextHandle, BrowserListObserver {
    private final String mName;
    private final boolean mIsIncognito;
    private long mNativeProfile;
    private CookieManagerImpl mCookieManager;
    private PrerenderControllerImpl mPrerenderController;
    private Runnable mOnDestroyCallback;
    private boolean mBeingDeleted;
    private boolean mDownloadsInitialized;
    private DownloadCallbackProxy mDownloadCallbackProxy;
    private GoogleAccountAccessTokenFetcherProxy mAccessTokenFetcherProxy;
    private IUserIdentityCallbackClient mUserIdentityCallbackClient;
    private IOpenUrlCallbackClient mOpenUrlCallbackClient;
    private List<Intent> mDownloadNotificationIntents = new ArrayList<>();

    private IProfileClient mClient;

    // If non-null, indicates when no browsers reference this Profile the Profile is destroyed. The
    // contents are the list of runnables supplied to destroyAndDeleteDataFromDiskSoon().
    private List<Runnable> mDelayedDestroyCallbacks;

    public static void enumerateAllProfileNames(ValueCallback<String[]> callback) {
        final Callback<String[]> baseCallback = (String[] names) -> callback.onReceiveValue(names);
        ProfileImplJni.get().enumerateAllProfileNames(baseCallback);
    }

    ProfileImpl(String name, boolean isIncognito, Runnable onDestroyCallback) {
        // Normal profiles have restrictions on the name.
        if (!isIncognito && !name.matches("^\\w+$")) {
            throw new IllegalArgumentException(
                    "Non-incognito profiles names can only contain words: " + name);
        }
        mIsIncognito = isIncognito;
        mName = name;
        mNativeProfile = ProfileImplJni.get().createProfile(name, ProfileImpl.this, mIsIncognito);
        mCookieManager = new CookieManagerImpl(
                ProfileImplJni.get().getCookieManager(mNativeProfile), ProfileImpl.this);
        mPrerenderController = new PrerenderControllerImpl(
                ProfileImplJni.get().getPrerenderController(mNativeProfile));
        mOnDestroyCallback = onDestroyCallback;
        mDownloadCallbackProxy = new DownloadCallbackProxy(this);
        mAccessTokenFetcherProxy = new GoogleAccountAccessTokenFetcherProxy(this);
    }

    private void destroyDependentJavaObjects() {
        if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }

        if (mAccessTokenFetcherProxy != null) {
            mAccessTokenFetcherProxy.destroy();
            mAccessTokenFetcherProxy = null;
        }

        if (mCookieManager != null) {
            mCookieManager.destroy();
            mCookieManager = null;
        }

        if (mPrerenderController != null) {
            mPrerenderController.destroy();
            mPrerenderController = null;
        }
        FontSizePrefs.destroyInstance();
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
        if (!canDestroyNow()) {
            throw new IllegalStateException("Profile still in use: " + mName);
        }

        final Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);
        destroyAndDeleteDataFromDiskImpl(callback);
    }

    private void destroyAndDeleteDataFromDiskImpl(Runnable callback) {
        assert canDestroyNow();
        mBeingDeleted = true;
        BrowserList.getInstance().removeObserver(this);
        destroyDependentJavaObjects();
        if (mClient != null) {
            try {
                mClient.onProfileDestroyed();
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        ProfileImplJni.get().destroyAndDeleteDataFromDisk(mNativeProfile, () -> {
            if (callback != null) callback.run();
            if (mDelayedDestroyCallbacks != null) {
                for (Runnable r : mDelayedDestroyCallbacks) r.run();
                mDelayedDestroyCallbacks = null;
            }
        });
        mNativeProfile = 0;
        maybeRunDestroyCallback();
    }

    private boolean canDestroyNow() {
        return ProfileImplJni.get().getNumBrowserImpl(mNativeProfile) == 0;
    }

    @Override
    public void destroyAndDeleteDataFromDiskSoon(IObjectWrapper completionCallback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        assert mNativeProfile != 0;
        if (canDestroyNow()) {
            destroyAndDeleteDataFromDisk(completionCallback);
            return;
        }
        // The profile is still in use. Wait for all browsers to be destroyed before cleaning up.
        if (mDelayedDestroyCallbacks == null) {
            BrowserList.getInstance().addObserver(this);
            mDelayedDestroyCallbacks = new ArrayList<>();
            ProfileImplJni.get().markAsDeleted(mNativeProfile);
        }
        final Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);
        if (callback != null) mDelayedDestroyCallbacks.add(callback);
    }

    @Override
    public void setClient(IProfileClient client) {
        mClient = client;
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

    @Override
    public void setUserIdentityCallbackClient(IUserIdentityCallbackClient client) {
        StrictModeWorkaround.apply();
        mUserIdentityCallbackClient = client;
    }

    public IUserIdentityCallbackClient getUserIdentityCallbackClient() {
        return mUserIdentityCallbackClient;
    }

    @Override
    public void setGoogleAccountAccessTokenFetcherClient(
            IGoogleAccountAccessTokenFetcherClient client) {
        StrictModeWorkaround.apply();
        mAccessTokenFetcherProxy.setClient(client);
    }

    @Override
    public void setTablessOpenUrlCallbackClient(IOpenUrlCallbackClient client) {
        StrictModeWorkaround.apply();
        mOpenUrlCallbackClient = client;
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    public boolean areDownloadsInitialized() {
        return mDownloadsInitialized;
    }

    public void addDownloadNotificationIntent(Intent intent) {
        mDownloadNotificationIntents.add(intent);
        ProfileImplJni.get().ensureBrowserContextInitialized(mNativeProfile);
    }

    @Override
    public void onBrowserCreated(BrowserImpl browser) {}

    @Override
    public void onBrowserDestroyed(BrowserImpl browser) {
        if (!canDestroyNow()) return;

        // onBrowserDestroyed() is called from the destructor of Browser. This is a rather fragile
        // time to destroy the profile. Delay.
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> {
            if (!mBeingDeleted && canDestroyNow()) {
                destroyAndDeleteDataFromDiskImpl(null);
            }
        });
    }

    @Override
    public void clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes, long fromMillis,
            long toMillis, @NonNull IObjectWrapper completionCallback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        // `toMillis` should be greater than `fromMillis`
        assert fromMillis < toMillis;
        // Handle ContentCapture data clearing.
        PlatformContentCaptureController controller =
                PlatformContentCaptureController.getInstance();
        if (controller != null) {
            for (int type : dataTypes) {
                if (type == BrowsingDataType.COOKIES_AND_SITE_DATA) {
                    controller.clearAllContentCaptureData();
                    break;
                }
            }
        }
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
    public IPrerenderController getPrerenderController() {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        return mPrerenderController;
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
        return CollectionUtil.integerCollectionToIntArray(convertedTypes);
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

    @CalledByNative
    public long getBrowserForNewTab() throws RemoteException {
        if (mOpenUrlCallbackClient == null) return 0;

        IBrowser browser = mOpenUrlCallbackClient.getBrowserForNewTab();
        if (browser == null) return 0;

        return ((BrowserImpl) browser).getNativeBrowser();
    }

    @CalledByNative
    public void onTabAdded(TabImpl tab) throws RemoteException {
        if (mOpenUrlCallbackClient == null) return;

        mOpenUrlCallbackClient.onTabAdded(tab.getId());
    }

    @Override
    public void setBooleanSetting(@SettingType int type, boolean value) {
        ProfileImplJni.get().setBooleanSetting(mNativeProfile, type, value);
    }

    @Override
    public boolean getBooleanSetting(@SettingType int type) {
        return ProfileImplJni.get().getBooleanSetting(mNativeProfile, type);
    }

    public void fetchAccessTokenForTesting(IObjectWrapper scopesWrapper,
            IObjectWrapper onTokenFetchedWrapper) throws RemoteException {
        mAccessTokenFetcherProxy.fetchAccessToken(ObjectWrapper.unwrap(scopesWrapper, Set.class),
                ObjectWrapper.unwrap(onTokenFetchedWrapper, ValueCallback.class));
    }

    public void fireOnAccessTokenIdentifiedAsInvalidForTesting(
            IObjectWrapper scopesWrapper, IObjectWrapper tokenWrapper) throws RemoteException {
        mAccessTokenFetcherProxy.onAccessTokenIdentifiedAsInvalid(
                ObjectWrapper.unwrap(scopesWrapper, Set.class),
                ObjectWrapper.unwrap(tokenWrapper, String.class));
    }

    @NativeMethods
    interface Natives {
        void enumerateAllProfileNames(Callback<String[]> callback);
        long createProfile(String name, ProfileImpl caller, boolean isIncognito);
        void deleteProfile(long profile);
        long getBrowserContext(long nativeProfileImpl);
        int getNumBrowserImpl(long nativeProfileImpl);
        void destroyAndDeleteDataFromDisk(long nativeProfileImpl, Runnable completionCallback);
        void clearBrowsingData(long nativeProfileImpl, @ImplBrowsingDataType int[] dataTypes,
                long fromMillis, long toMillis, Runnable callback);
        void setDownloadDirectory(long nativeProfileImpl, String directory);
        long getCookieManager(long nativeProfileImpl);
        long getPrerenderController(long nativeProfileImpl);
        void ensureBrowserContextInitialized(long nativeProfileImpl);
        void setBooleanSetting(long nativeProfileImpl, int type, boolean value);
        boolean getBooleanSetting(long nativeProfileImpl, int type);
        void getBrowserPersistenceIds(long nativeProfileImpl, Callback<String[]> callback);
        void removeBrowserPersistenceStorage(
                long nativeProfileImpl, String[] ids, Callback<Boolean> callback);
        void prepareForPossibleCrossOriginNavigation(long nativeProfileImpl);
        void getCachedFaviconForPageUrl(
                long nativeProfileImpl, String url, Callback<Bitmap> callback);
        void markAsDeleted(long nativeProfileImpl);
    }
}
