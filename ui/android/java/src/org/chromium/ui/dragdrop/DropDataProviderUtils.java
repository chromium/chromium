// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.chromium.ui.dragdrop.DropDataProviderImpl.CACHE_METHOD_NAME;
import static org.chromium.ui.dragdrop.DropDataProviderImpl.ON_DRAG_END_METHOD_NAME;
import static org.chromium.ui.dragdrop.DropDataProviderImpl.SET_INTERVAL_METHOD_NAME;

import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;

/**
 * This class wraps all the calls to ContentResolver#call.
 *
 */
public class DropDataProviderUtils {
    /**
     * Wraps the call to onDragEnd in the provider, we call it to clear the cached image data after
     * dragging ends
     *
     * @return Whether the image cache was successfully cleared.
     */
    static boolean clearImageCache(boolean imageIsInUse) {
        Bundle bundle = new Bundle();
        bundle.putBoolean("imageIsInUse", imageIsInUse);
        try {
            ContextUtils.getApplicationContext()
                    .getContentResolver()
                    .call(DropDataProviderImpl.FULL_AUTH_URI, ON_DRAG_END_METHOD_NAME, "", bundle);
            return true;
        } catch (NullPointerException | IllegalArgumentException exception) {
            return false;
        }
    }

    /** Wraps the call to setClearCachedDataIntervalMs in the provider. */
    public static boolean setClearCachedDataIntervalMs(int delay) {
        Bundle bundle = new Bundle();
        bundle.putInt(DropDataProviderImpl.CLEAR_CACHE_PARAM, delay);
        try {
            ContextUtils.getApplicationContext()
                    .getContentResolver()
                    .call(DropDataProviderImpl.FULL_AUTH_URI, SET_INTERVAL_METHOD_NAME, "", bundle);
            return true;
        } catch (NullPointerException | IllegalArgumentException exception) {
            return false;
        }
    }

    /**
     * Wraps the call to cache in the provider and returns the cached Uri or null if it failed to
     * call the content provider.
     */
    @Nullable
    static Uri cacheImageData(DropDataAndroid dropData) {
        Bundle bundle = new Bundle();
        bundle.putSerializable(DropDataProviderImpl.BYTES_PARAM, dropData.imageContent);
        bundle.putString(
                DropDataProviderImpl.IMAGE_CONTENT_EXTENSION_PARAM, dropData.imageContentExtension);

        bundle.putString(DropDataProviderImpl.IMAGE_FILE_PARAM, dropData.imageFilename);
        try {
            Bundle cachedUriBundle =
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .call(
                                    DropDataProviderImpl.FULL_AUTH_URI,
                                    CACHE_METHOD_NAME,
                                    "",
                                    bundle);
            return cachedUriBundle.getParcelable("uri");
        } catch (NullPointerException | IllegalArgumentException exception) {
            return null;
        }
    }
}
