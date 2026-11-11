// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.chromium.ui.dragdrop.DropDataProviderImpl.CACHE_METHOD_NAME;
import static org.chromium.ui.dragdrop.DropDataProviderImpl.GET_IMAGE_METHOD_NAME;
import static org.chromium.ui.dragdrop.DropDataProviderImpl.ON_DRAG_END_METHOD_NAME;
import static org.chromium.ui.dragdrop.DropDataProviderImpl.SET_INTERVAL_METHOD_NAME;
import static org.chromium.ui.dragdrop.DropDataProviderImpl.URI_PARAM;

import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Wraps calls to this app's DropDataProvider via {@link android.content.ContentResolver#call}
 * (rather than a process-local singleton, as WebView embedders may declare the provider in another
 * process of the same app).
 */
@NullMarked
public class DropDataProviderUtils {
    /** Returns whether {@code uri} is served by this app's DropDataProvider. */
    public static boolean isDropDataProviderUri(Uri uri) {
        // Both the authority and the authority of the round-trip Uri.parse(Uri.toString()) are
        // checked since android has had security bugs where Uri.getAuthority() can be different
        // from the value used in Uri.toString(). Any such mismatch is rejected.
        String authority = DropDataProviderImpl.FULL_AUTH_URI.getAuthority();
        return TextUtils.equals(authority, uri.getAuthority())
                && TextUtils.equals(authority, Uri.parse(uri.toString()).getAuthority());
    }

    /** The image this app's DropDataProvider is serving for the drag in progress. */
    public static class CachedImage {
        public final byte[] bytes;
        public final String extension;
        public final String filename;

        CachedImage(byte[] bytes, String extension, String filename) {
            this.bytes = bytes;
            this.extension = extension;
            this.filename = filename;
        }
    }

    /**
     * Wraps the call to onDragEnd in the provider, we call it to clear the cached image data after
     * dragging ends
     *
     * @return Whether the image cache was successfully cleared.
     */
    @VisibleForTesting
    public static boolean clearImageCache(boolean imageIsInUse) {
        Bundle bundle = new Bundle();
        bundle.putBoolean(DropDataProviderImpl.IMAGE_USAGE_PARAM, imageIsInUse);
        try {
            ContextUtils.getApplicationContext()
                    .getContentResolver()
                    .call(DropDataProviderImpl.FULL_AUTH_URI, ON_DRAG_END_METHOD_NAME, "", bundle);
            return true;
        } catch (NullPointerException | IllegalArgumentException | SecurityException exception) {
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
        } catch (NullPointerException | IllegalArgumentException | SecurityException exception) {
            return false;
        }
    }

    /**
     * Wraps the call to cache in the provider and returns the cached Uri or null if it failed to
     * call the content provider.
     */
    static @Nullable Uri cacheImageData(DropDataAndroid dropData) {
        if (dropData.imageContent == null
                || dropData.imageContentExtension == null
                || dropData.imageFilename == null) {
            return null;
        }
        Bundle bundle = new Bundle();
        bundle.putByteArray(DropDataProviderImpl.BYTES_PARAM, dropData.imageContent);
        bundle.putString(
                DropDataProviderImpl.IMAGE_CONTENT_EXTENSION_PARAM, dropData.imageContentExtension);
        bundle.putString(DropDataProviderImpl.IMAGE_FILE_PARAM, dropData.imageFilename);
        bundle.putString(DropDataProviderImpl.DRAG_ID_PARAM, dropData.dragId);
        try {
            Bundle cachedUriBundle =
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .call(
                                    DropDataProviderImpl.FULL_AUTH_URI,
                                    CACHE_METHOD_NAME,
                                    "",
                                    bundle);
            if (cachedUriBundle == null) {
                return null;
            }
            return cachedUriBundle.getParcelable(URI_PARAM);
        } catch (NullPointerException | IllegalArgumentException | SecurityException exception) {
            return null;
        }
    }

    /**
     * Returns the image behind {@code uri} if it is the one this app's DropDataProvider is
     * currently serving, i.e. the image being dragged out of one of this app's own web contents,
     * and {@code dragId} is the drag ID that drag was started with. Returns null for any other URI,
     * and if {@code dragId} is null or doesn't match.
     */
    public static @Nullable CachedImage getCachedImage(Uri uri, @Nullable String dragId) {
        if (dragId == null || !isDropDataProviderUri(uri)) {
            return null;
        }
        Bundle bundle = new Bundle();
        bundle.putParcelable(URI_PARAM, uri);
        bundle.putString(DropDataProviderImpl.DRAG_ID_PARAM, dragId);
        try {
            Bundle imageBundle =
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .call(
                                    DropDataProviderImpl.FULL_AUTH_URI,
                                    GET_IMAGE_METHOD_NAME,
                                    "",
                                    bundle);
            if (imageBundle == null) {
                return null;
            }
            byte[] bytes = imageBundle.getByteArray(DropDataProviderImpl.BYTES_PARAM);
            String extension =
                    imageBundle.getString(DropDataProviderImpl.IMAGE_CONTENT_EXTENSION_PARAM);
            String filename = imageBundle.getString(DropDataProviderImpl.IMAGE_FILE_PARAM);
            if (bytes == null || extension == null || filename == null) {
                return null;
            }
            return new CachedImage(bytes, extension, filename);
        } catch (NullPointerException | IllegalArgumentException | SecurityException exception) {
            return null;
        }
    }
}
