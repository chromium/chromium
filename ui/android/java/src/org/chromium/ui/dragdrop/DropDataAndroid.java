// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/** Bare minimal wrapper class of native content::DropData. */
@JNINamespace("ui")
@NullMarked
public class DropDataAndroid {
    public final @Nullable String text;
    public final @Nullable GURL gurl;
    public final byte @Nullable [] imageContent;
    public final @Nullable String imageContentExtension;
    public final @Nullable String imageFilename;

    protected DropDataAndroid(
            @Nullable String text,
            @Nullable GURL gurl,
            byte @Nullable [] imageContent,
            @Nullable String imageContentExtension,
            @Nullable String imageFilename) {
        this.text = text;
        this.gurl = gurl;
        this.imageContent = imageContent;
        this.imageContentExtension = imageContentExtension;
        this.imageFilename = imageFilename;
    }

    @VisibleForTesting
    @CalledByNative
    static DropDataAndroid create(
            String text,
            GURL gurl,
            byte[] imageContent,
            String imageContentExtension,
            String imageFilename) {
        return new DropDataAndroid(text, gurl, imageContent, imageContentExtension, imageFilename);
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
        return imageContent != null
                && !TextUtils.isEmpty(imageContentExtension)
                && !TextUtils.isEmpty(imageFilename);
    }

    /** Return whether this data presents browser content. */
    public boolean hasBrowserContent() {
        return false;
    }
}
