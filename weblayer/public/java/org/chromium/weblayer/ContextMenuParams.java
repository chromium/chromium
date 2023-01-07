// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.IContextMenuParams;

/**
 * Parameters for constructing context menu.
 */
class ContextMenuParams {
    /**
     * The Uri associated with the main frame of the page that triggered the context menu.
     */
    @NonNull
    public final Uri pageUri;

    /**
     * The link Uri, if any.
     */
    @Nullable
    public final Uri linkUri;

    /**
     * The link text, if any.
     */
    @Nullable
    public final String linkText;

    /**
     * The title or alt attribute (if title is not available).
     */
    @Nullable
    public final String titleOrAltText;

    /**
     * This is the source Uri for the element that the context menu was
     * invoked on.  Example of elements with source URLs are img, audio, and
     * video.
     */
    @Nullable
    public final Uri srcUri;

    /**
     * Whether srcUri points to an image.
     *
     * @since 88
     */
    public final boolean isImage;

    /**
     * Whether srcUri points to a video.
     *
     * @since 88
     */
    public final boolean isVideo;

    /**
     * Whether this link or image/video can be downloaded. Only if this is true can
     * {@link Tab.download} be called.
     *
     * @since 88
     */
    public final boolean canDownload;

    final IContextMenuParams mContextMenuParams;

    protected ContextMenuParams(
            Uri pageUri, Uri linkUri, String linkText, String titleOrAltText, Uri srcUri) {
        this(pageUri, linkUri, linkText, titleOrAltText, srcUri, false, false, false, null);
    }

    protected ContextMenuParams(Uri pageUri, Uri linkUri, String linkText, String titleOrAltText,
            Uri srcUri, boolean isImage, boolean isVideo, boolean canDownload,
            IContextMenuParams contextMenuParams) {
        this.pageUri = pageUri;
        this.linkUri = linkUri;
        this.linkText = linkText;
        this.titleOrAltText = titleOrAltText;
        this.srcUri = srcUri;
        this.isImage = isImage;
        this.isVideo = isVideo;
        this.canDownload = canDownload;
        this.mContextMenuParams = contextMenuParams;
    }
}
