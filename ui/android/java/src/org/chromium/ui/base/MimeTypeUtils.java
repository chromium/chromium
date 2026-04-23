// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.base;

import android.content.ClipDescription;
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
    /** The MIME type for a tab object dragged from Chrome. */
    public static final String CHROME_MIMETYPE_TAB = "chrome/tab";

    /** The MIME type for a multi-tab object dragged from Chrome. */
    public static final String CHROME_MIMETYPE_MULTI_TAB = "chrome/multi-tab";

    /** The MIME type for a tab group object dragged from Chrome. */
    public static final String CHROME_MIMETYPE_TAB_GROUP = "chrome/tab-group";

    /** The MIME type for any image. */
    public static final String IMAGE_ANY_MIME_TYPE = "image/*";

    /** The MIME type for pdf. */
    public static final String PDF_MIME_TYPE = "application/pdf";

    /** The MIME type prefix for any image. */
    public static final String IMAGE_PREFIX_MIME_TYPE = "image/";

    /** The MIME type prefix for any text file. */
    public static final String TEXT_PREFIX_MIME_TYPE = "text/";

    /** The MIME type prefix for any audio. */
    public static final String AUDIO_PREFIX_MIME_TYPE = "audio/";

    /** The MIME type prefix for any video. */
    public static final String VIDEO_PREFIX_MIME_TYPE = "video/";

    /** The MIME type for any file type. */
    public static final String ALL_FILE_TYPES_MIME_TYPE = "*/*";

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
     * @param mimeType A string representing the MIME type (e.g., "image/png").
     * @return The corresponding {@link Type}.
     */
    public static @Type int getTypeFromMimeType(@Nullable String mimeType) {
        if (mimeType == null) return Type.UNKNOWN;
        if (mimeType.startsWith(TEXT_PREFIX_MIME_TYPE)) {
            return Type.TEXT;
        } else if (mimeType.startsWith(IMAGE_PREFIX_MIME_TYPE)) {
            return Type.IMAGE;
        } else if (mimeType.startsWith(AUDIO_PREFIX_MIME_TYPE)) {
            return Type.AUDIO;
        } else if (mimeType.startsWith(VIDEO_PREFIX_MIME_TYPE)) {
            return Type.VIDEO;
        } else if (mimeType.equals(PDF_MIME_TYPE)) {
            return Type.PDF;
        }
        return Type.UNKNOWN;
    }

    /**
     * @param url A {@link GURL} for which to determine the mime type.
     * @return The mime type, based on the extension of the {@code url}.
     */
    public static @Type int getMimeTypeForUrl(GURL url) {
        String extension = MimeTypeMap.getFileExtensionFromUrl(url.getSpec());
        if (extension == null) return Type.UNKNOWN;
        String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
        return getTypeFromMimeType(type);
    }

    public static boolean clipDescriptionHasBrowserContent(ClipDescription clipDescription) {
        if (clipDescription == null) return false;
        return clipDescription.hasMimeType(CHROME_MIMETYPE_TAB)
                || clipDescription.hasMimeType(CHROME_MIMETYPE_TAB_GROUP)
                || clipDescription.hasMimeType(CHROME_MIMETYPE_MULTI_TAB);
    }
}
