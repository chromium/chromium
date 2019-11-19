// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Profile holds state (typically on disk) needed for browsing. Create a
 * Profile via WebLayer.
 */
public final class Profile {
    private static final Map<String, Profile> sProfiles = new HashMap<>();

    /* package */ static Profile of(IProfile impl) {
        ThreadCheck.ensureOnUiThread();
        String path;
        try {
            path = impl.getPath();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        Profile profile = sProfiles.get(path);
        if (profile != null) {
            return profile;
        }

        return new Profile(path, impl);
    }

    /**
     * Return all profiles that have been created and not yet called destroyed.
     * Note returned structure is immutable.
     */
    @NonNull
    public static Collection<Profile> getAllProfiles() {
        ThreadCheck.ensureOnUiThread();
        return Collections.unmodifiableCollection(sProfiles.values());
    }

    private final String mPath;
    private IProfile mImpl;

    private Profile(String path, IProfile impl) {
        mPath = path;
        mImpl = impl;

        sProfiles.put(path, this);
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
     * @return {@link ListenableResult} into which a "null" will be supplied when clearing is
     * finished.
     */
    @NonNull
    public ListenableResult<Void> clearBrowsingData(
            @NonNull @BrowsingDataType int[] dataTypes, long fromMillis, long toMillis) {
        ThreadCheck.ensureOnUiThread();
        try {
            ListenableResult<Void> result = new ListenableResult<>();
            mImpl.clearBrowsingData(dataTypes, fromMillis, toMillis,
                    ObjectWrapper.wrap((Runnable) () -> result.supplyResult(null)));
            return result;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Clears the data associated with the Profile.
     * Same as {@link #clearBrowsingData(int[], long, long)} with unbounded time range.
     */
    @NonNull
    public ListenableResult<Void> clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes) {
        return clearBrowsingData(dataTypes, 0, Long.MAX_VALUE);
    }

    public void destroy() {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.destroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        mImpl = null;
        sProfiles.remove(mPath);
    }
}
