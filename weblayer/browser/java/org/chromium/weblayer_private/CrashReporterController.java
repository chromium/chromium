// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.crash.browser.ChildProcessCrashObserver;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploader;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Map;

/**
 * CrashReporterController handles crashes in weblayer_private and delegates them to the
 * CrashReceiverService from android_webview non-embedded code.
 */
public final class CrashReporterController {
    private static final String TAG = "CrashReporter";
    private static final int MAX_UPLOAD_RETRIES = 3;

    private CrashFileManager mCrashFileManager;
    private boolean mIsNativeInitialized;

    private static class Holder {
        static CrashReporterController sInstance = new CrashReporterController();
    }

    private CrashReporterController() {}

    public static CrashReporterController getInstance() {
        return Holder.sInstance;
    }

    public void notifyNativeInitialized() {
        mIsNativeInitialized = true;

        processNewMinidumps();
        TraceEvent.instant(TAG, "Start observing child process crashes");
        ChildProcessCrashObserver.registerCrashCallback(
                new ChildProcessCrashObserver.ChildCrashedCallback() {
                    @Override
                    public void childCrashed(int pid) {
                        TraceEvent.instant(TAG, "Child process crashed. Process new minidumps.");
                        processNewMinidumps();
                    }
                });
    }

    public void deleteCrash(String localId) {
        StrictModeWorkaround.apply();
        AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> { deleteCrashOnBackgroundThread(localId); });
    }

    public void uploadCrash(String localId) {
        StrictModeWorkaround.apply();
        AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> {
            TraceEvent.instant(TAG, "CrashReporterController: Begin uploading crash");
            File minidumpFile = getCrashFileManager().getCrashFileWithLocalId(localId);
            if (minidumpFile == null) {
                return;
            }
            MinidumpUploader.Result result = new MinidumpUploader().upload(minidumpFile);
            if (result.isSuccess()) {
                CrashFileManager.markUploadSuccess(minidumpFile);
            } else {
                CrashFileManager.tryIncrementAttemptNumber(minidumpFile);
            }

            TraceEvent.instant(TAG,
                    "CrashReporterController: Crash upload "
                            + (result.isSuccess() ? "succeeded" : "failed"));
        });
    }

    public @Nullable Bundle getCrashKeys(String localId) {
        StrictModeWorkaround.apply();
        JSONObject data = readSidecar(localId);
        if (data == null) {
            return null;
        }
        Bundle result = new Bundle();
        Iterator<String> iter = data.keys();
        while (iter.hasNext()) {
            String key = iter.next();
            try {
                result.putCharSequence(key, data.getString(key));
            } catch (JSONException e) {
                // Skip non-string values.
            }
        }
        return result;
    }

    /** Start an async task to import crashes, and notify if any are found. */
    private void processNewMinidumps() {
        AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> { processNewMinidumpsOnBackgroundThread(); });
    }

    /** Delete a crash report (and any sidecar file) given its local ID. */
    private void deleteCrashOnBackgroundThread(String localId) {
        File minidumpFile = getCrashFileManager().getCrashFileWithLocalId(localId);
        File sidecarFile = sidecarFile(localId);
        if (minidumpFile != null) {
            CrashFileManager.deleteFile(minidumpFile);
        }
        if (sidecarFile != null) {
            CrashFileManager.deleteFile(sidecarFile);
        }
    }

    /**
     * Determine the set of crashes that are currently ready to be uploaded.
     *
     * Clean out crashes that are too old, and return the any remaining crashes that have not
     * exceeded their upload retry limit.
     *
     * @return An array of local IDs for crashes that are ready to be uploaded.
     */
    private String[] getPendingMinidumpsOnBackgroundThread() {
        TraceEvent.instant(
                TAG, "CrashReporterController: Start determining crashes ready to be uploaded.");
        getCrashFileManager().cleanOutAllNonFreshMinidumpFiles();
        File[] pendingMinidumps =
                getCrashFileManager().getMinidumpsReadyForUpload(MAX_UPLOAD_RETRIES);
        ArrayList<String> localIds = new ArrayList<>(pendingMinidumps.length);
        for (File minidump : pendingMinidumps) {
            localIds.add(CrashFileManager.getCrashLocalIdFromFileName(minidump.getName()));
        }
        TraceEvent.instant(
                TAG, "CrashReporterController: Finish determinining crashes ready to be uploaded.");
        return localIds.toArray(new String[0]);
    }

    /**
     * Use the CrashFileManager to import crashes from crashpad.
     *
     * For each imported crash, a sidecar file (in JSON format) is written, containing the
     * crash keys that were recorded at the time of the crash.
     *
     * @return An array of local IDs of the new crashes (may be empty).
     */
    private String[] processNewMinidumpsOnBackgroundThread() {
        TraceEvent.instant(
                TAG, "CrashReporterController: Start processing minidumps in the background.");
        Map<String, Map<String, String>> crashesInfoMap =
                getCrashFileManager().importMinidumpsCrashKeys();
        if (crashesInfoMap == null) return new String[0];
        ArrayList<String> localIds = new ArrayList<>(crashesInfoMap.size());
        for (Map.Entry<String, Map<String, String>> entry : crashesInfoMap.entrySet()) {
            JSONObject crashKeysJson = new JSONObject(entry.getValue());
            String uuid = entry.getKey();
            // TODO(tobiasjs): the minidump uploader uses the last component of the uuid as
            // the local ID. The ergonomics of this should be improved.
            localIds.add(CrashFileManager.getCrashLocalIdFromFileName(uuid + ".dmp"));
            writeSidecar(uuid, crashKeysJson);
        }
        for (File minidump : getCrashFileManager().getMinidumpsSansLogcat()) {
            CrashFileManager.trySetReadyForUpload(minidump);
        }
        TraceEvent.instant(
                TAG, "CrashReporterController: Finish processing minidumps in the background.");
        return localIds.toArray(new String[0]);
    }

    /**
     * Generate a sidecar file path given a crash local ID.
     *
     * The sidecar file holds a JSON representation of the crash keys associated
     * with the crash. All crash keys and values are strings.
     */
    private @Nullable File sidecarFile(String localId) {
        File minidumpFile = getCrashFileManager().getCrashFileWithLocalId(localId);
        if (minidumpFile == null) {
            return null;
        }
        String uuid = minidumpFile.getName().split("\\.")[0];
        return new File(minidumpFile.getParent(), uuid + ".json");
    }

    /** Write JSON formatted crash key data to the sidecar file for a crash. */
    private void writeSidecar(String localId, JSONObject data) {
        File sidecar = sidecarFile(localId);
        if (sidecar == null) {
            return;
        }
        try (FileOutputStream out = new FileOutputStream(sidecar)) {
            out.write(data.toString().getBytes("UTF-8"));
        } catch (IOException e) {
            Log.w(TAG, "Failed to write crash keys JSON for crash " + localId);
            sidecar.delete();
        }
    }

    /** Read JSON formatted crash key data previously written to a crash sidecar file. */
    private @Nullable JSONObject readSidecar(String localId) {
        File sidecar = sidecarFile(localId);
        if (sidecar == null) {
            return null;
        }
        try (FileInputStream in = new FileInputStream(sidecar)) {
            byte[] data = new byte[(int) sidecar.length()];
            int offset = 0;
            while (offset < data.length) {
                int count = in.read(data, offset, data.length - offset);
                if (count <= 0) break;
                offset += count;
            }
            return new JSONObject(new String(data, "UTF-8"));
        } catch (IOException | JSONException e) {
            return null;
        }
    }

    private CrashFileManager getCrashFileManager() {
        if (mCrashFileManager == null) {
            File cacheDir = new File(PathUtils.getCacheDirectory());
            // Make sure the cache dir has been created, since this may be called before WebLayer
            // has been initialized.
            cacheDir.mkdir();
            mCrashFileManager = new CrashFileManager(cacheDir);
        }
        return mCrashFileManager;
    }
}
