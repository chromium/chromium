// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.ICrashReporterController;
import org.chromium.weblayer_private.interfaces.ICrashReporterControllerClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Provides an API to allow WebLayer embedders to control the handling of crash reports.
 *
 * Creation of the CrashReporterController singleton bootstraps WebLayer code loading (but not full
 * initialization) so that it can be used in a lightweight fashion from a scheduled task.
 *
 * Crash reports are identified by a unique string, with which is associated an opaque blob of data
 * for upload, and a key: value dictionary of crash metadata. Given an identifier, callers can
 * either trigger an upload attempt, or delete the crash report.
 *
 * After successful upload, local crash data is deleted and the crash can be referenced by the
 * returned remote identifier string.
 *
 * An embedder should register a CrashReporterCallback to be alerted to the presence of crash
 * reports via {@link CrashReporterCallback#onPendingCrashReports}. When a crash report is
 * available, the embedder should decide whether the crash should be uploaded or deleted based on
 * user preference. Knowing that a crash is available can be used as a signal to schedule upload
 * work for a later point in time (or favourable power/network conditions).
 */
class CrashReporterController {
    private ICrashReporterController mImpl;
    private final ObserverList<CrashReporterCallback> mCallbacks;

    private static final class Holder {
        static CrashReporterController sInstance = new CrashReporterController();
    }

    // Protected so it's available for test mocking.
    protected CrashReporterController() {
        mCallbacks = new ObserverList<CrashReporterCallback>();
    }

    /** Retrieve the singleton CrashReporterController instance. */
    public static CrashReporterController getInstance(@NonNull Context appContext) {
        ThreadCheck.ensureOnUiThread();
        return Holder.sInstance.connect(appContext.getApplicationContext());
    }

    /**
     * Asynchronously fetch the set of crash reports ready to be uploaded.
     *
     * Results are returned via {@link CrashReporterCallback#onPendingCrashReports}.
     */
    public void checkForPendingCrashReports() {
        try {
            mImpl.checkForPendingCrashReports();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Get the crash keys for a crash.
     *
     * Performs IO on this thread, so should not be called on the UI thread.
     *
     * @param localId a crash identifier.
     * @return a Bundle containing crash information as key: value pairs.
     */
    public @Nullable Bundle getCrashKeys(String localId) {
        try {
            return mImpl.getCrashKeys(localId);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Asynchronously delete a crash.
     *
     * Deletion is notified via {@link CrashReporterCallback#onCrashDeleted}
     *
     * @param localId a crash identifier.
     */
    public void deleteCrash(@NonNull String localId) {
        try {
            mImpl.deleteCrash(localId);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Asynchronously upload a crash.
     *
     * Success is notified via {@link CrashReporterCallback#onCrashUploadSucceeded},
     * and the crash report is purged. On upload failure,
     * {@link CrashReporterCallback#onCrashUploadFailed} is called. The crash report
     * will be kept until it is deemed too old, or too many upload attempts have
     * failed.
     *
     * @param localId a crash identifier.
     */
    public void uploadCrash(@NonNull String localId) {
        try {
            mImpl.uploadCrash(localId);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /** Register a {@link CrashReporterCallback} callback object. */
    public void registerCallback(@NonNull CrashReporterCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.addObserver(callback);
    }

    /** Unregister a previously registered {@link CrashReporterCallback} callback object. */
    public void unregisterCallback(@NonNull CrashReporterCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.removeObserver(callback);
    }

    private CrashReporterController connect(@NonNull Context appContext) {
        if (mImpl != null) {
            return this;
        }
        try {
            mImpl = WebLayer.getIWebLayer(appContext)
                            .getCrashReporterController(ObjectWrapper.wrap(appContext),
                                    ObjectWrapper.wrap(
                                            WebLayer.getOrCreateRemoteContext(appContext)));
            mImpl.setClient(new CrashReporterControllerClientImpl());
        } catch (Exception e) {
            throw new APICallException(e);
        }
        return this;
    }

    private final class CrashReporterControllerClientImpl
            extends ICrashReporterControllerClient.Stub {
        @Override
        public void onPendingCrashReports(String[] localIds) {
            StrictModeWorkaround.apply();
            for (CrashReporterCallback callback : mCallbacks) {
                callback.onPendingCrashReports(localIds);
            }
        }

        @Override
        public void onCrashDeleted(String localId) {
            StrictModeWorkaround.apply();
            for (CrashReporterCallback callback : mCallbacks) {
                callback.onCrashDeleted(localId);
            }
        }

        @Override
        public void onCrashUploadSucceeded(String localId, String reportId) {
            StrictModeWorkaround.apply();
            for (CrashReporterCallback callback : mCallbacks) {
                callback.onCrashUploadSucceeded(localId, reportId);
            }
        }

        @Override
        public void onCrashUploadFailed(String localId, String failureReason) {
            StrictModeWorkaround.apply();
            for (CrashReporterCallback callback : mCallbacks) {
                callback.onCrashUploadFailed(localId, failureReason);
            }
        }
    }
}
