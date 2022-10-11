// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientDownload;
import org.chromium.weblayer_private.interfaces.IDownload;

import java.io.File;

/**
 * Contains information about a single download that's in progress.
 */
class Download extends IClientDownload.Stub {
    private final IDownload mDownloadImpl;

    // Constructor for test mocking.
    protected Download() {
        mDownloadImpl = null;
    }

    Download(IDownload impl) {
        mDownloadImpl = impl;
    }

    /**
     * By default downloads will show a system notification. Call this to disable it.
     */
    public void disableNotification() {
        ThreadCheck.ensureOnUiThread();
        try {
            mDownloadImpl.disableNotification();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @DownloadState
    public int getState() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mDownloadImpl.getState();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the total number of expected bytes. Returns -1 if the total size is not known.
     */
    public long getTotalBytes() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mDownloadImpl.getTotalBytes();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Total number of bytes that have been received and written to the download file.
     */
    public long getReceivedBytes() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mDownloadImpl.getReceivedBytes();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Pauses the download.
     */
    public void pause() {
        ThreadCheck.ensureOnUiThread();
        try {
            mDownloadImpl.pause();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Resumes the download.
     */
    public void resume() {
        ThreadCheck.ensureOnUiThread();
        try {
            mDownloadImpl.resume();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Cancels the download.
     */
    public void cancel() {
        ThreadCheck.ensureOnUiThread();
        try {
            mDownloadImpl.cancel();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the location of the downloaded file. This may be empty if the target path hasn't been
     * determined yet. The file it points to won't be available until the download completes
     * successfully.
     */
    @NonNull
    public File getLocation() {
        ThreadCheck.ensureOnUiThread();
        try {
            return new File(mDownloadImpl.getLocation());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the file name for the download that should be displayed to the user.
     */
    @NonNull
    public File getFileNameToReportToUser() {
        ThreadCheck.ensureOnUiThread();
        try {
            return new File(mDownloadImpl.getFileNameToReportToUser());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the effective MIME type of downloaded content.
     */
    @NonNull
    public String getMimeType() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mDownloadImpl.getMimeType();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Return information about the error, if any, that was encountered during the download.
     */
    @DownloadError
    public int getError() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mDownloadImpl.getError();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
