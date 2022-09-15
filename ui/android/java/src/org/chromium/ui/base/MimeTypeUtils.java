// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.base;

import android.Manifest.permission;
import android.webkit.MimeTypeMap;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.ui.permissions.PermissionConstants;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Utility methods for determining and working with mime types.
 */
public class MimeTypeUtils {
    /**
     * A set of known mime types.
     */
    // Note: these values must match the AndroidUtilsMimeTypes enum in enums.xml.
    // Only add new values at the end, right before NUM_ENTRIES. We depend on these specific
    // values in UMA histograms.
    @IntDef({Type.UNKNOWN, Type.TEXT, Type.IMAGE, Type.AUDIO, Type.VIDEO, Type.PDF})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int UNKNOWN = 0;
        int TEXT = 1;
        int IMAGE = 2;
        int AUDIO = 3;
        int VIDEO = 4;
        int PDF = 5;
    }

    /** The number of entries in {@link Type}. */
    public static final int NUM_MIME_TYPE_ENTRIES = 6;

    /**
     * @param url A {@link GURL} for which to determine the mime type.
     * @return The mime type, based on the extension of the {@code url}.
     */
    public static @Type int getMimeTypeForUrl(GURL url) {
        String extension = MimeTypeMap.getFileExtensionFromUrl(url.getSpec());
        @Type
        int mimeType = Type.UNKNOWN;
        if (extension != null) {
            String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            if (type != null) {
                if (type.startsWith("text")) {
                    mimeType = Type.TEXT;
                } else if (type.startsWith("image")) {
                    mimeType = Type.IMAGE;
                } else if (type.startsWith("audio")) {
                    mimeType = Type.AUDIO;
                } else if (type.startsWith("video")) {
                    mimeType = Type.VIDEO;
                } else if (type.equals("application/pdf")) {
                    mimeType = Type.PDF;
                }
            }
        }

        return mimeType;
    }

    /**
     * @param mimeType The mime type associated with an operation that needs a permission.
     * @return The name of the Android permission to request. Returns null if no permission will
     *         allow access to the file, for example on Android T+ where READ_EXTERNAL_STORAGE has
     *         been replaced with a handful of READ_MEDIA_* permissions.
     */
    public @Nullable static String getPermissionNameForMimeType(@MimeTypeUtils.Type int mimeType) {
        if (useExternalStoragePermission()) {
            return permission.READ_EXTERNAL_STORAGE;
        }

        switch (mimeType) {
            case MimeTypeUtils.Type.AUDIO:
                return PermissionConstants.READ_MEDIA_AUDIO;
            case MimeTypeUtils.Type.IMAGE:
                return PermissionConstants.READ_MEDIA_IMAGES;
            case MimeTypeUtils.Type.VIDEO:
                return PermissionConstants.READ_MEDIA_VIDEO;
            default:
                return null;
        }
    }

    static boolean useExternalStoragePermission() {
        // Extracted into a helper method for easy testing. Can be replaced with test annotations
        // once Robolectric recognizes SDK = T.
        return !BuildInfo.isAtLeastT() || !BuildInfo.targetsAtLeastT();
    }
}
