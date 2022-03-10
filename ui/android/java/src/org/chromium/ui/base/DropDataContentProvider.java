// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.OutputStream;

/** ContentProvider for image data of Drag and Drop. */
// TODO(crbug.com/1302386): Make DropDataContentProvider thread safety.
public class DropDataContentProvider extends ContentProvider {
    /**
     * Implement {@link ContentProvider.PipeDataWriter} to be used by {@link
     * ContentProvider#openPipeHelper}, in order to stream image data to drop target.
     */
    private static class DropPipeDataWriter implements ContentProvider.PipeDataWriter<Void> {
        @Override
        public void writeDataToPipe(
                ParcelFileDescriptor output, Uri uri, String mimeType, Bundle opts, Void unused) {
            try (OutputStream out = new FileOutputStream(output.getFileDescriptor())) {
                if (sImageBytes != null) {
                    out.write(sImageBytes);
                } else {
                    // TODO: add error handle here
                }
                out.flush();
            } catch (Exception e) {
            }
        }
    }

    public static final int DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS = 60_000;
    // TODO: move ConversionUtils.java to //ui/android/ to use ConversionUtils.BYTES_PER_KILOBYTE
    public static final int BYTES_PER_KILOBYTE = 1024;

    private static final String[] COLUMNS = {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
    private static final String URI_AUTHORITY_SUFFIX = ".DropDataProvider";
    private static int sClearCachedDataIntervalMs = DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS;
    private static byte[] sImageBytes;
    private static String sEncodingFormat;
    /** The URI handled by this content provider. */
    private static Uri sContentProviderUri;
    private static String sTimestamp;
    private static Handler sHandler;
    private static long sDragEndTime;
    private static long sOpenFileLastAccessTime;
    private static Uri sLastUri;
    private static long sLastUriClearedTimestamp;
    private static long sLastUriCreatedTimestamp;
    private static boolean sLastUriRecorded;
    private DropPipeDataWriter mDropPipeDataWriter;

    /**
     * Update the delayed time before clearing the image cache.
     */
    public static void setClearCachedDataIntervalMs(int milliseconds) {
        sClearCachedDataIntervalMs = milliseconds;
    }

    /**
     * Cache the passed-in image data of Drag and Drop.
     */
    static Uri cache(byte[] imageBytes, String encodingFormat) {
        // Clear out any old data.
        clearCacheData();

        // Set new data.
        long elapsedRealtime = SystemClock.elapsedRealtime();
        if (sLastUriCreatedTimestamp > 0) {
            long duration = elapsedRealtime - sLastUriCreatedTimestamp;
            RecordHistogram.recordMediumTimesHistogram(
                    "Android.DragDrop.Image.UriCreatedInterval", duration);
        }
        sLastUriCreatedTimestamp = elapsedRealtime;
        sImageBytes = imageBytes;
        sEncodingFormat = encodingFormat;
        sTimestamp = String.valueOf(System.currentTimeMillis());
        sDragEndTime = 0;
        sOpenFileLastAccessTime = 0;
        // TODO(crbug.com/1296795): Replace path with filename with extension
        Uri newUri = new Uri.Builder()
                             .scheme(ContentResolver.SCHEME_CONTENT)
                             .authority(ContextUtils.getApplicationContext().getPackageName()
                                     + URI_AUTHORITY_SUFFIX)
                             .path(sTimestamp)
                             .build();
        sContentProviderUri = newUri;
        int sizeInKB = imageBytes.length / BYTES_PER_KILOBYTE;
        RecordHistogram.recordCustomCountHistogram(
                "Android.DragDrop.Image.Size", sizeInKB, 1, 100_000, 50);
        return newUri;
    }

    /**
     * Clear the image data of Drag and Drop when event ACTION_DRAG_ENDED is received.
     *
     * @param imageInUse Indicate if the image is needed by the drop target app. This is true when
     *         the image is dropped outside of Chrome AND the drop target app returns true for event
     *         ACTION_DROP.
     */
    static void onDragEnd(boolean imageInUse) {
        if (!imageInUse) {
            // Clear the image data immediately when:
            // 1. Image is dropped within Clank and we know it is not used;
            // 2. Image is dropped outside of Clank and the drop target app rejects the data.
            clearCache();
        } else {
            // Otherwise, clear it with a delay to allow asynchronous data transfer.
            clearCacheWithDelay();
            sDragEndTime = SystemClock.elapsedRealtime();
        }
    }

    /**
     * Clear the image data of Drag and Drop and record histogram.
     */
    static void clearCache() {
        clearCacheData();
        if (sDragEndTime > 0 && sDragEndTime <= sOpenFileLastAccessTime) {
            long duration = sOpenFileLastAccessTime - sDragEndTime;
            RecordHistogram.recordMediumTimesHistogram(
                    "Android.DragDrop.Image.OpenFileTime.LastAttempt", duration);
        }
    }

    /**
     * Clear the image data of Drag and Drop.
     */
    private static void clearCacheData() {
        sImageBytes = null;
        sEncodingFormat = null;
        if (sContentProviderUri != null) {
            sLastUri = sContentProviderUri;
            sLastUriClearedTimestamp = SystemClock.elapsedRealtime();
            sLastUriRecorded = false;
        }
        sContentProviderUri = null;
        sTimestamp = null;
        if (sHandler != null) {
            sHandler.removeCallbacksAndMessages(null);
            sHandler = null;
        }
    }

    /**
     * Clear the image data of Drag and Drop with delay.
     */
    private static void clearCacheWithDelay() {
        if (sHandler == null) {
            sHandler = new Handler(Looper.getMainLooper());
        }
        sHandler.postDelayed(DropDataContentProvider::clearCache, sClearCachedDataIntervalMs);
    }

    /**
     * Initialize the provider.
     * This is one of the required method to implement a concrete subclass of ContentProvider.
     */
    @Override
    public boolean onCreate() {
        // TODO(crbug.com/1302383): Lazily create DropPipeDataWriter in #openFile.
        mDropPipeDataWriter = new DropPipeDataWriter();
        return true;
    }

    @Override
    public String getType(Uri uri) {
        if (uri == null || !uri.equals(sContentProviderUri)) {
            return null;
        }

        return MimeTypeMap.getSingleton().getMimeTypeFromExtension(sEncodingFormat);
    }

    @Override
    public String[] getStreamTypes(Uri uri, String mimeTypeFilter) {
        if (uri == null || !uri.equals(sContentProviderUri)) {
            return null;
        }
        String mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(sEncodingFormat);
        return matchMimeType(mimeType, mimeTypeFilter) ? new String[] {mimeType} : null;
    }

    private boolean matchMimeType(String mimeType, String mimeTypeFilter) {
        if (mimeType == null || mimeTypeFilter == null) {
            return false;
        }

        int idx1 = mimeType.indexOf('/');
        String type = mimeType.substring(0, idx1);
        String subtype = mimeType.substring(idx1 + 1);
        int idx2 = mimeTypeFilter.indexOf('/');
        String typeFilter = mimeTypeFilter.substring(0, idx2);
        String subtypeFilter = mimeTypeFilter.substring(idx2 + 1);
        if (!typeFilter.equals("*") && !typeFilter.equals(type)) {
            return false;
        }
        return subtypeFilter.equals("*") || subtypeFilter.equals(subtype);
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (uri == null) {
            return null;
        }
        if (!uri.equals(sContentProviderUri)) {
            if (uri.equals(sLastUri)) {
                long duration = SystemClock.elapsedRealtime() - sLastUriClearedTimestamp;
                RecordHistogram.recordMediumTimesHistogram(
                        "Android.DragDrop.Image.OpenFileTime.AllExpired", duration);
                if (!sLastUriRecorded) {
                    RecordHistogram.recordMediumTimesHistogram(
                            "Android.DragDrop.Image.OpenFileTime.FirstExpired", duration);
                    sLastUriRecorded = true;
                }
            }
            return null;
        }
        sOpenFileLastAccessTime = SystemClock.elapsedRealtime();
        return openPipeHelper(
                sContentProviderUri, getType(sContentProviderUri), null, null, mDropPipeDataWriter);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        if (uri == null || !uri.equals(sContentProviderUri)) {
            return new MatrixCursor(COLUMNS, 0);
        }
        if (projection == null) {
            projection = COLUMNS;
        }

        boolean hasDisplayName = false;
        boolean hasSize = false;
        int length = 0;
        for (String col : projection) {
            if (OpenableColumns.DISPLAY_NAME.equals(col)) {
                hasDisplayName = true;
                length++;
            } else if (OpenableColumns.SIZE.equals(col)) {
                hasSize = true;
                length++;
            }
        }

        String[] cols = new String[length];
        Object[] values = new Object[length];
        int index = 0;
        if (hasDisplayName) {
            cols[index] = OpenableColumns.DISPLAY_NAME;
            // TODO(crbug.com/1296795): Use the real file name from DropDataAndroid
            values[index] = sTimestamp + "." + sEncodingFormat;
            index++;
        }
        if (hasSize) {
            cols[index] = OpenableColumns.SIZE;
            values[index] = sImageBytes.length;
        }
        MatrixCursor cursor = new MatrixCursor(cols, 1);
        cursor.addRow(values);
        return cursor;
    }

    @Override
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @VisibleForTesting
    static byte[] getImageBytesForTesting() {
        return sImageBytes;
    }

    @VisibleForTesting
    static Handler getHandlerForTesting() {
        return sHandler;
    }

    @VisibleForTesting
    static void clearLastUriCreatedTimestampForTesting() {
        sLastUriCreatedTimestamp = 0;
    }
}
