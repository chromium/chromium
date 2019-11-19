// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * An interface that allows clients to handle download requests originating in the browser.
 */
public abstract class DownloadCallback {
    /**
     * A download of has been requested with the specified details.
     *
     * @param url the target that should be downloaded
     * @param userAgent the user agent to be used for the download
     * @param contentDisposition content-disposition http header, if present
     * @param mimetype the mimetype of the content reported by the server
     * @param contentLength the file size reported by the server
     */
    public abstract void onDownloadRequested(@NonNull String url, @NonNull String userAgent,
            @NonNull String contentDisposition, @NonNull String mimetype, long contentLength);
}
