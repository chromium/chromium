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
    public static final String EXTRA_CUSTOM_DATA = "org.chromium.ui.dragdrop.customData";
    public static final String EXTRA_EFFECT_ALLOWED = "org.chromium.ui.dragdrop.effectAllowed";

    /**
     * ClipDescription extra carrying the drag ID (content::DropData::drag_id) of a drag started
     * from web content. Unlike ClipData items, ClipDescription extras are available to drop targets
     * from ACTION_DRAG_STARTED until the drop, so the drag can be identified before the drop. Drags
     * without ClipData, such as DOM element moves, don't carry it.
     *
     * <p>Any app can read this extra or set it on its own drags, so a received value is a claim,
     * not proof that the drag came from this browser.
     */
    public static final String EXTRA_DRAG_ID = "org.chromium.ui.dragdrop.dragId";

    public final @Nullable String text;
    public final @Nullable GURL gurl;
    public final byte @Nullable [] imageContent;
    public final @Nullable String imageContentExtension;
    public final @Nullable String imageFilename;
    public final @Nullable String customData;
    public final @Nullable String effectAllowed;
    public final @Nullable String dragId;

    protected DropDataAndroid(
            @Nullable String text,
            @Nullable GURL gurl,
            byte @Nullable [] imageContent,
            @Nullable String imageContentExtension,
            @Nullable String imageFilename,
            @Nullable String customData,
            @Nullable String effectAllowed) {
        this(
                text,
                gurl,
                imageContent,
                imageContentExtension,
                imageFilename,
                customData,
                effectAllowed,
                /* dragId= */ null);
    }

    protected DropDataAndroid(
            @Nullable String text,
            @Nullable GURL gurl,
            byte @Nullable [] imageContent,
            @Nullable String imageContentExtension,
            @Nullable String imageFilename,
            @Nullable String customData,
            @Nullable String effectAllowed,
            @Nullable String dragId) {
        this.text = text;
        this.gurl = gurl;
        this.imageContent = imageContent;
        this.imageContentExtension = imageContentExtension;
        this.imageFilename = imageFilename;
        this.customData = customData;
        this.effectAllowed = effectAllowed;
        this.dragId = dragId;
    }

    @VisibleForTesting
    static DropDataAndroid create(
            @Nullable String text,
            @Nullable GURL gurl,
            byte @Nullable [] imageContent,
            @Nullable String imageContentExtension,
            @Nullable String imageFilename,
            @Nullable String customData,
            @Nullable String effectAllowed) {
        return create(
                text,
                gurl,
                imageContent,
                imageContentExtension,
                imageFilename,
                customData,
                effectAllowed,
                /* dragId= */ null);
    }

    @VisibleForTesting
    @CalledByNative
    static DropDataAndroid create(
            @Nullable String text,
            @Nullable GURL gurl,
            byte @Nullable [] imageContent,
            @Nullable String imageContentExtension,
            @Nullable String imageFilename,
            @Nullable String customData,
            @Nullable String effectAllowed,
            @Nullable String dragId) {
        return new DropDataAndroid(
                text,
                gurl,
                imageContent,
                imageContentExtension,
                imageFilename,
                customData,
                effectAllowed,
                dragId);
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

    /** Return whether this data presents custom data. */
    public boolean hasCustomData() {
        return !TextUtils.isEmpty(customData);
    }
}
