// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.graphics.Bitmap;
import android.net.Uri;
import android.os.RemoteException;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientDownload;
import org.chromium.weblayer_private.interfaces.IDownload;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IProfileClient;
import org.chromium.weblayer_private.interfaces.IUserIdentityCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.io.File;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Profile holds state (typically on disk) needed for browsing. Create a
 * Profile via WebLayer.
 */
public class Profile {
    private static final Map<String, Profile> sProfiles = new HashMap<>();
    private static final Map<String, Profile> sIncognitoProfiles = new HashMap<>();

    /* package */ static Profile of(IProfile impl) {
        ThreadCheck.ensureOnUiThread();
        String name;
        try {
            name = impl.getName();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        boolean isIncognito;
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            isIncognito = "".equals(name);
        } else {
            try {
                isIncognito = impl.isIncognito();
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        Profile profile;
        if (isIncognito) {
            profile = sIncognitoProfiles.get(name);
        } else {
            profile = sProfiles.get(name);
        }
        if (profile != null) {
            return profile;
        }

        return new Profile(name, impl, isIncognito);
    }

    /**
     * Return all profiles that have been created and not yet called destroyed.
     */
    @NonNull
    public static Collection<Profile> getAllProfiles() {
        ThreadCheck.ensureOnUiThread();
        Set<Profile> profiles = new HashSet<Profile>();
        profiles.addAll(sProfiles.values());
        profiles.addAll(sIncognitoProfiles.values());
        return profiles;
    }

    private final String mName;
    private final boolean mIsIncognito;
    private IProfile mImpl;
    private DownloadCallbackClientImpl mDownloadCallbackClient;
    private final CookieManager mCookieManager;
    private final PrerenderController mPrerenderController;

    // Constructor for test mocking.
    protected Profile() {
        mName = null;
        mIsIncognito = false;
        mImpl = null;
        mCookieManager = null;
        mPrerenderController = null;
    }

    private Profile(String name, IProfile impl, boolean isIncognito) {
        mName = name;
        mImpl = impl;
        mIsIncognito = isIncognito;
        mCookieManager = CookieManager.create(impl);
        if (WebLayer.getSupportedMajorVersionInternal() >= 87) {
            mPrerenderController = PrerenderController.create(impl);
        } else {
            mPrerenderController = null;
        }

        if (isIncognito) {
            sIncognitoProfiles.put(name, this);
        } else {
            sProfiles.put(name, this);
        }

        if (WebLayer.getSupportedMajorVersionInternal() >= 87) {
            try {
                mImpl.setClient(new ProfileClientImpl());
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
    }

    /**
     * Returns the name of the profile. While added in 88, this can be used with any version.
     *
     * @return The name of the profile.
     *
     * @since 87
     */
    @NonNull
    public String getName() {
        return mName;
    }

    /**
     * Returns true if the profile is incognito. While added in 88, this can be used with any
     * version.
     *
     * @return True if the profile is incognito.
     *
     * @since 87
     */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /**
     * Clears the data associated with the Profile.
     * The clearing is asynchronous, and new data may be generated during clearing. It is safe to
     * call this method repeatedly without waiting for callback.
     *
     * @param dataTypes See {@link BrowsingDataType}.
     * @param fromMillis Defines the start (in milliseconds since epoch) of the time range to clear.
     * @param toMillis Defines the end (in milliseconds since epoch) of the time range to clear.
     * For clearing all data prefer using {@link Long#MAX_VALUE} to
     * {@link System.currentTimeMillis()} to take into account possible system clock changes.
     * @param callback {@link Runnable} which is run when clearing is finished.
     */
    public void clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes, long fromMillis,
            long toMillis, @NonNull Runnable callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.clearBrowsingData(dataTypes, fromMillis, toMillis, ObjectWrapper.wrap(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Clears the data associated with the Profile.
     * Same as {@link #clearBrowsingData(int[], long, long, Runnable)} with unbounded time range.
     */
    public void clearBrowsingData(
            @NonNull @BrowsingDataType int[] dataTypes, @NonNull Runnable callback) {
        ThreadCheck.ensureOnUiThread();
        clearBrowsingData(dataTypes, 0, Long.MAX_VALUE, callback);
    }

    /**
     * Delete all profile data stored on disk. There are a number of edge cases with deleting
     * profile data:
     * * This method will throw an exception if there are any existing usage of this Profile. For
     *   example, all BrowserFragment belonging to this profile must be destroyed.
     * * This object is considered destroyed after this method returns. Calling any other method
     *   after will throw exceptions.
     * * Creating a new profile of the same name before doneCallback runs will throw an exception.
     * @since 82
     */
    public void destroyAndDeleteDataFromDisk(@Nullable Runnable completionCallback) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.destroyAndDeleteDataFromDisk(ObjectWrapper.wrap(completionCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        onDestroyed();
    }

    /**
     * This method provides the same functionality as {@link destroyAndDeleteDataFromDisk}, but
     * delays until there is no usage of the Profile. If there is no usage of the profile
     * destruction is immediate. If there is usage, then destruction happens as soon as possible.
     * It's possible cleanup may not happen until WebLayer is restarted. If cleanup does not happen
     * until WebLayer is restarted, {@link completionCallback} is not run (because the process was
     * restarted).
     *
     * While destruction may be delayed, once this function is called, the profile name will not be
     * returned from {@link WebLayer#enumerateAllProfileNames}.
     *
     * @param completionCallback Callback this is notified when destruction is complete. This may
     *         never be called.
     *
     * @since 87
     */
    public void destroyAndDeleteDataFromDiskSoon(@Nullable Runnable completionCallback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        try {
            mImpl.destroyAndDeleteDataFromDiskSoon(ObjectWrapper.wrap(completionCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private void throwIfDestroyed() {
        if (mImpl == null) {
            throw new IllegalStateException("Profile can not be used once destroyed");
        }
    }

    @Deprecated
    public void destroy() {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.destroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        onDestroyed();
    }

    private void onDestroyed() {
        if (mIsIncognito) {
            sIncognitoProfiles.remove(mName);
        } else {
            sProfiles.remove(mName);
        }
        mImpl = null;
    }

    /**
     * Allows embedders to override the default download directory. By default this is the system
     * download directory.
     *
     * @param directory the directory to place downloads in.
     *
     * @since 81
     */
    public void setDownloadDirectory(@NonNull File directory) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.setDownloadDirectory(directory.toString());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Allows embedders to control how downloads function.
     *
     * @param callback the callback interface implemented by the embedder.
     *
     * @since 83
     */
    public void setDownloadCallback(@Nullable DownloadCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (callback != null) {
                mDownloadCallbackClient = new DownloadCallbackClientImpl(callback);
                mImpl.setDownloadCallbackClient(mDownloadCallbackClient);
            } else {
                mDownloadCallbackClient = null;
                mImpl.setDownloadCallbackClient(null);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Gets the cookie manager for this profile.
     *
     * @since 83
     */
    @NonNull
    public CookieManager getCookieManager() {
        ThreadCheck.ensureOnUiThread();

        return mCookieManager;
    }

    /**
     * Gets the prerender controller for this profile.
     *
     * @since 88
     */
    @NonNull
    public PrerenderController getPrerenderController() {
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            throw new UnsupportedOperationException();
        }

        ThreadCheck.ensureOnUiThread();
        return mPrerenderController;
    }

    /**
     * Allows the embedder to set a boolean value for a specific setting, see {@link SettingType}
     * for more details and the possible options.
     *
     * @param type See {@link SettingType}.
     * @param value The value to set for the setting.
     *
     * @since 84
     */
    public void setBooleanSetting(@SettingType int type, boolean value) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.setBooleanSetting(type, value);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the current value for the given setting type, see {@link SettingType} for more
     * details and the possible options.
     *
     * @since 84
     */
    public boolean getBooleanSetting(@SettingType int type) {
        ThreadCheck.ensureOnUiThread();
        try {
            return mImpl.getBooleanSetting(type);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Asynchronously fetches the set of known Browser persistence-ids. See
     * {@link WebLayer#createBrowserFragment} for details on the persistence-id.
     *
     * @param callback The callback that is supplied the set of ids.
     *
     * @throws IllegalStateException If called on an in memory profile.
     *
     * @since 85
     */
    public void getBrowserPersistenceIds(@NonNull Callback<Set<String>> callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 85) {
            throw new UnsupportedOperationException();
        }
        if (mName.isEmpty()) {
            throw new IllegalStateException(
                    "getBrowserPersistenceIds() is not applicable to in-memory profiles");
        }
        try {
            mImpl.getBrowserPersistenceIds(
                    ObjectWrapper.wrap((ValueCallback<Set<String>>) callback::onResult));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Asynchronously removes the storage associated with the set of Browser persistence-ids. This
     * ignores ids actively in use. {@link doneCallback} is supplied the result of the operation. A
     * value of true means all files were removed. A value of false indicates at least one of the
     * files could not be removed.
     *
     * @param callback The callback that is supplied the result of the operation.
     *
     * @throws IllegalStateException If called on an in memory profile.
     * @throws IllegalArgumentException if {@link ids} contains an empty/null string.
     *
     * @since 85
     */
    public void removeBrowserPersistenceStorage(
            @NonNull Set<String> ids, @NonNull Callback<Boolean> callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 85) {
            throw new UnsupportedOperationException();
        }
        if (mName.isEmpty()) {
            throw new IllegalStateException(
                    "removetBrowserPersistenceStorage() is not applicable to in-memory profiles");
        }
        try {
            mImpl.removeBrowserPersistenceStorage(ids.toArray(new String[ids.size()]),
                    ObjectWrapper.wrap((ValueCallback<Boolean>) callback::onResult));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * For cross-origin navigations, the implementation may leverage a separate OS process for
     * stronger isolation. If an embedder knows that a cross-origin navigation is likely starting
     * soon, they can call this method as a hint to the implementation to start a fresh OS process.
     * A subsequent navigation may use this preinitialized process, improving performance. It is
     * safe to call this multiple times or when it is not certain that the spare renderer will be
     * used, although calling this too eagerly may reduce performance as unnecessary processes are
     * created.
     *
     * @since 85
     */
    public void prepareForPossibleCrossOriginNavigation() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 85) {
            throw new UnsupportedOperationException();
        }

        try {
            mImpl.prepareForPossibleCrossOriginNavigation();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the previously downloaded favicon for {@link uri}.
     *
     * @param uri The uri to get the favicon for.
     * @param callback The callback that is notified of the bitmap. The bitmap passed to the
     * callback will be null if one is not available.
     *
     * @since 86
     */
    public void getCachedFaviconForPageUri(@NonNull Uri uri, @NonNull Callback<Bitmap> callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 86) {
            throw new UnsupportedOperationException();
        }
        try {
            mImpl.getCachedFaviconForPageUri(
                    uri.toString(), ObjectWrapper.wrap((ValueCallback<Bitmap>) callback::onResult));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * See {@link UserIdentityCallback}.
     *
     * @since 87
     */
    public void setUserIdentityCallback(@Nullable UserIdentityCallback callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            throw new UnsupportedOperationException();
        }
        try {
            mImpl.setUserIdentityCallbackClient(
                    callback == null ? null : new UserIdentityCallbackClientImpl(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    static final class DownloadCallbackClientImpl extends IDownloadCallbackClient.Stub {
        private final DownloadCallback mCallback;

        DownloadCallbackClientImpl(DownloadCallback callback) {
            mCallback = callback;
        }

        public DownloadCallback getCallback() {
            return mCallback;
        }

        @Override
        public boolean interceptDownload(String uriString, String userAgent,
                String contentDisposition, String mimetype, long contentLength) {
            StrictModeWorkaround.apply();
            return mCallback.onInterceptDownload(
                    Uri.parse(uriString), userAgent, contentDisposition, mimetype, contentLength);
        }

        @Override
        public void allowDownload(String uriString, String requestMethod,
                String requestInitiatorString, IObjectWrapper valueCallback) {
            StrictModeWorkaround.apply();
            Uri requestInitiator;
            if (requestInitiatorString != null) {
                requestInitiator = Uri.parse(requestInitiatorString);
            } else {
                requestInitiator = Uri.EMPTY;
            }
            mCallback.allowDownload(Uri.parse(uriString), requestMethod, requestInitiator,
                    (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                            valueCallback, ValueCallback.class));
        }

        @Override
        public IClientDownload createClientDownload(IDownload downloadImpl) {
            StrictModeWorkaround.apply();
            return new Download(downloadImpl);
        }

        @Override
        public void downloadStarted(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadStarted((Download) download);
        }

        @Override
        public void downloadProgressChanged(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadProgressChanged((Download) download);
        }

        @Override
        public void downloadCompleted(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadCompleted((Download) download);
        }

        @Override
        public void downloadFailed(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadFailed((Download) download);
        }
    }

    private static final class UserIdentityCallbackClientImpl
            extends IUserIdentityCallbackClient.Stub {
        private UserIdentityCallback mCallback;

        UserIdentityCallbackClientImpl(UserIdentityCallback callback) {
            mCallback = callback;
        }

        @Override
        public String getEmail() {
            StrictModeWorkaround.apply();
            return mCallback.getEmail();
        }

        @Override
        public String getFullName() {
            StrictModeWorkaround.apply();
            return mCallback.getFullName();
        }

        @Override
        public void getAvatar(int desiredSize, IObjectWrapper avatarLoadedWrapper) {
            StrictModeWorkaround.apply();
            ValueCallback<Bitmap> avatarLoadedCallback =
                    (ValueCallback<Bitmap>) ObjectWrapper.unwrap(
                            avatarLoadedWrapper, ValueCallback.class);
            mCallback.getAvatar(desiredSize, avatarLoadedCallback);
        }
    }

    private final class ProfileClientImpl extends IProfileClient.Stub {
        @Override
        public void onProfileDestroyed() {
            onDestroyed();
        }
    }
}
