// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.Context;
import android.text.TextUtils;
import android.view.DragEvent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.url.GURL;

/**
 * Bare minimal wrapper class of native content::DropData.
 */
@JNINamespace("ui")
public class DropDataAndroid {
    public final String text;
    public final GURL gurl;
    public final byte[] imageContent;
    public final String imageContentExtension;
    public final String imageFilename;

    /** MIME types that used to pass from Android into natives. */
    public final @Nullable String[] mimeTypes;

    /** Empty instance that used for placeholder */
    private static DropDataAndroid sEmptyInstance;

    private DropDataAndroid(String text, GURL gurl, byte[] imageContent,
            String imageContentExtension, String imageFilename, @Nullable String[] mimeTypes) {
        this.text = text;
        this.gurl = gurl;
        this.imageContent = imageContent;
        this.imageContentExtension = imageContentExtension;
        this.imageFilename = imageFilename;

        this.mimeTypes = mimeTypes;
    }

    /** Get an empty instance of {@link DropDataAndroid} that is used as placeholder. */
    public static DropDataAndroid emptyInstance() {
        if (sEmptyInstance == null) {
            sEmptyInstance = new DropDataAndroid("", null, null, "", "", new String[0]);
        }
        return sEmptyInstance;
    }

    @VisibleForTesting
    @CalledByNative
    static DropDataAndroid create(String text, GURL gurl, byte[] imageContent,
            String imageContentExtension, String imageFilename) {
        return new DropDataAndroid(
                text, gurl, imageContent, imageContentExtension, imageFilename, null);
    }

    /** Return whether this data presents a plain of text. */
    public boolean isPlainText() {
        return GURL.isEmptyOrInvalid(gurl) && !TextUtils.isEmpty(text);
    }

    /** Return whether this data presents a link. */
    public boolean hasLink() {
        return !GURL.isEmptyOrInvalid(gurl);
    }

    /** Return whether this data presents an image. */
    public boolean hasImage() {
        return imageContent != null && !TextUtils.isEmpty(imageContentExtension)
                && !TextUtils.isEmpty(imageFilename);
    }

    /**
     * Create a DropDataAndroid from clip data from Android.
     * @param clipData {@link DragEvent#getClipData()}
     * @param context The caller's Context, from which its ContentResolver and other things can be
     *         retrieved.
     * @return A DropDataAndroid presentation of given clipData.
     */
    public static DropDataAndroid createFromClipData(ClipData clipData, Context context) {
        ClipDescription clipDescription = clipData.getDescription();
        if (clipDescription == null) {
            return emptyInstance();
        }

        // Attempt to match text.
        String[] mimeTypes = clipDescription.filterMimeTypes("text/*");

        if (mimeTypes == null) {
            // Attempt to match images.
            // TODO(https://crbug.com/1261249): parse the right data if mimetypes matched for
            // images.
            mimeTypes = clipDescription.filterMimeTypes("image/*");
        }

        if (mimeTypes == null) {
            return emptyInstance();
        }

        StringBuilder content = new StringBuilder("");
        final int itemCount = clipData.getItemCount();
        for (int i = 0; i < itemCount; i++) {
            ClipData.Item item = clipData.getItemAt(i);
            content.append(item.coerceToStyledText(context));
        }
        return new DropDataAndroid(content.toString(), null, null, "", "", mimeTypes);
    }

    /**
     * @param clipDescription See {@link DragEvent#getClipDescription()}
     * @return Whether the {@link ClipData} presented by the {@link ClipDescription} is supported.
     */
    public static boolean isClipContentSupported(ClipDescription clipDescription) {
        // text/* will match text/uri-list, text/html, text/plain.
        String[] mimeTypes =
                clipDescription == null ? new String[0] : clipDescription.filterMimeTypes("text/*");
        // mimeTypes is null iff there is no matching text MIME type.
        // Try if there is any matching image MIME type.
        if (mimeTypes == null) {
            mimeTypes = clipDescription.filterMimeTypes("image/*");
        }

        return mimeTypes != null && mimeTypes.length > 0;
    }

    // Native getters

    @CalledByNative
    private String getText() {
        return text;
    }
}
