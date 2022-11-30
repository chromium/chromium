// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An interface that allows clients to handle download requests originating in the browser.
 */
abstract class DownloadCallback {
    /**
     * A download of has been requested with the specified details. If it returns true the download
     * will be considered intercepted and WebLayer won't proceed with it. Note that there are many
     * corner cases where the embedder downloading it won't work (e.g. POSTs, one-time URLs,
     * requests that depend on cookies or auth state). This is called after AllowDownload.
     *
     * @param uri the target that should be downloaded
     * @param userAgent the user agent to be used for the download
     * @param contentDisposition content-disposition http header, if present
     * @param mimetype the mimetype of the content reported by the server
     * @param contentLength the file size reported by the server
     */
    public abstract boolean onInterceptDownload(@NonNull Uri uri, @NonNull String userAgent,
            @NonNull String contentDisposition, @NonNull String mimetype, long contentLength);

    /**
     * Gives the embedder the opportunity to asynchronously allow or disallow the
     * given download. It's safe to run |callback| synchronously.
     *
     * @param uri the target that is being downloaded
     * @param requestMethod the method (GET/POST etc...) of the download
     * @param requestInitiator the initiating Uri, if present
     * @param callback a callback to allow or disallow the download. Must be called to avoid leaks,
     *         and must be called on the UI thread.
     */
    public abstract void allowDownload(@NonNull Uri uri, @NonNull String requestMethod,
            @Nullable Uri requestInitiator, @NonNull ValueCallback<Boolean> callback);

    /**
     * A download has started. There will be 0..n calls to DownloadProgressChanged, then either a
     * call to DownloadCompleted or DownloadFailed. The same |download| will be provided on
     * subsequent calls to those methods when related to this download. Observers should clear any
     * references to |download| in onDownloadCompleted or onDownloadFailed, just before it is
     * destroyed.
     *
     * @param download the unique object for this download.
     */
    public void onDownloadStarted(@NonNull Download download) {}

    /**
     * The download progress has changed.
     *
     * @param download the unique object for this download.
     */
    public void onDownloadProgressChanged(@NonNull Download download) {}

    /**
     * The download has completed successfully.
     *
     * Note that |download| will be destroyed at the end of this call, so do not keep a reference
     * to it afterward.
     *
     * @param download the unique object for this download.
     */
    public void onDownloadCompleted(@NonNull Download download) {}

    /**
     * The download has failed because the user cancelled it or because of a server or network
     * error.
     *
     * Note that |download| will be destroyed at the end of this call, so do not keep a reference
     * to it afterward.
     *
     * @param download the unique object for this download.
     */
    public void onDownloadFailed(@NonNull Download download) {}
}
