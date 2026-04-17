// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.base;

import android.content.ClipDescription;
import android.webkit.MimeTypeMap;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
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

    public static boolean clipDescriptionHasBrowserContent(ClipDescription clipDescription) {
        if (clipDescription == null) return false;
        return clipDescription.hasMimeType(CHROME_MIMETYPE_TAB)
                || clipDescription.hasMimeType(CHROME_MIMETYPE_TAB_GROUP)
                || clipDescription.hasMimeType(CHROME_MIMETYPE_MULTI_TAB);
    }
}
