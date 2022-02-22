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
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.OutputStream;

/** ContentProvider for image data of Drag and Drop. */
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

    private static final String[] COLUMNS = {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
    private static final String URI_AUTHORITY_SUFFIX = ".DropDataProvider";
    private static int sClearCachedDataIntervalMs = DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS;
    private static byte[] sImageBytes;
    private static String sEncodingFormat;
    /** The URI handled by this content provider. */
    private static Uri sContentProviderUri;
    private static String sTimestamp;
    private static Handler sHandler;
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
    public static Uri cache(byte[] imageBytes, String encodingFormat) {
        // Cancel pending task if any to avoid new image data being cleared.
        if (sHandler != null) {
            sHandler.removeCallbacksAndMessages(null);
            sHandler = null;
        }
        sImageBytes = imageBytes;
        sEncodingFormat = encodingFormat;
        sTimestamp = String.valueOf(System.currentTimeMillis());
        // TODO(crbug.com/1296795): Replace path with filename with extension
        Uri newUri = new Uri.Builder()
                             .scheme(ContentResolver.SCHEME_CONTENT)
                             .authority(ContextUtils.getApplicationContext().getPackageName()
                                     + URI_AUTHORITY_SUFFIX)
                             .path(sTimestamp)
                             .build();
        sContentProviderUri = newUri;
        // TODO: add metric on image data size
        return newUri;
    }

    /**
     * Clear the image data of Drag and Drop.
     */
    public static void clearCache() {
        sImageBytes = null;
        sEncodingFormat = null;
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
    public static void clearCacheWithDelay() {
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
        if (uri == null || !uri.equals(sContentProviderUri)) {
            return null;
        }
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
    public static byte[] getImageBytesForTesting() {
        return sImageBytes;
    }

    @VisibleForTesting
    public static Handler getHandlerForTesting() {
        return sHandler;
    }
}
