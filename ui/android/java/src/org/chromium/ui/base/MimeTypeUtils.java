// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.base;

import android.Manifest;
import android.content.ClipDescription;
import android.os.Build;
import android.webkit.MimeTypeMap;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility methods for determining and working with mime types. */
@NullMarked
public class MimeTypeUtils {
    /** The MIME type for a plain text objects dragged from Chrome. */
    public static final String CHROME_MIMETYPE_TEXT = "chrome/text";

    /** The MIME type for a link objects dragged from Chrome. */
    public static final String CHROME_MIMETYPE_LINK = "chrome/link";

    /** The MIME type for a tab object dragged from Chrome. */
    public static final String CHROME_MIMETYPE_TAB = "chrome/tab";

    /** The MIME type for a tab group object dragged from Chrome. */
    public static final String CHROME_MIMETYPE_TAB_GROUP = "chrome/tab-group";

    /** The MIME type for pdf. */
    public static final String PDF_MIME_TYPE = "application/pdf";

    /** A set of known mime types. */
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
        @Type int mimeType = Type.UNKNOWN;
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
     *     allow access to the file, for example on Android T+ where READ_EXTERNAL_STORAGE has been
     *     replaced with a handful of READ_MEDIA_* permissions.
     */
    public @Nullable static String getPermissionNameForMimeType(@MimeTypeUtils.Type int mimeType) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return Manifest.permission.READ_EXTERNAL_STORAGE;
        }

        switch (mimeType) {
            case MimeTypeUtils.Type.AUDIO:
                return Manifest.permission.READ_MEDIA_AUDIO;
            case MimeTypeUtils.Type.IMAGE:
                return Manifest.permission.READ_MEDIA_IMAGES;
            case MimeTypeUtils.Type.VIDEO:
                return Manifest.permission.READ_MEDIA_VIDEO;
            default:
                return null;
        }
    }

    public static boolean clipDescriptionHasBrowserContent(ClipDescription clipDescription) {
        if (clipDescription == null) return false;
        return clipDescription.hasMimeType(CHROME_MIMETYPE_TAB)
                || clipDescription.hasMimeType(CHROME_MIMETYPE_TAB_GROUP);
    }
}
