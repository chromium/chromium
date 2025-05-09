// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.res.AssetFileDescriptor;
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

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.UsedByReflection;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.OutputStream;

/**
 * This class is the core implementation of Drag/Drop, we access it from the content provider for
 * each class loader, the chromium one {@link DropDataContentProvider}.
 *
 * @see DropDataProviderImpl#FULL_AUTH_URI
 *     <p>TODO(crbug.com/40235067): Add the reference to //android_webview/support_library content
 *     provider to this java doc.
 */
@UsedByReflection("Webview Support Lib")
@NullMarked
public class DropDataProviderImpl {
    public static final String CACHE_METHOD_NAME = "cache";
    public static final String SET_INTERVAL_METHOD_NAME = "setClearCachedDataIntervalMs";
    public static final String ON_DRAG_END_METHOD_NAME = "onDragEnd";

    public static final String CLEAR_CACHE_PARAM = "clearCacheDelayedMs";
    public static final String IMAGE_USAGE_PARAM = "imageIsInUse";
    public static final String BYTES_PARAM = "bytes";
    public static final String IMAGE_CONTENT_EXTENSION_PARAM = "imageContentExtension";
    public static final String IMAGE_FILE_PARAM = "imageFilename";
    public static final int DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS = 60_000;

    /**
     * This variable is being used to be able to access the correct content provider, any content
     * provider using this class should declare the same authority in order for it to work.
     */
    public static final Uri FULL_AUTH_URI =
            Uri.parse(
                    "content://"
                            + ContextUtils.getApplicationContext().getPackageName()
                            + DropDataProviderImpl.URI_AUTHORITY_SUFFIX);

    /**
     * Implement {@link ContentProvider.PipeDataWriter} to be used by {@link
     * ContentProvider#openPipeHelper}, in order to stream image data to drop target.
     */
    private static class DropPipeDataWriter implements ContentProvider.PipeDataWriter<byte[]> {
        @Override
        public void writeDataToPipe(
                ParcelFileDescriptor output,
                Uri uri,
                String mimeType,
                @Nullable Bundle opts,
                byte @Nullable [] imageBytes) {
            try (OutputStream out = new FileOutputStream(output.getFileDescriptor())) {
                if (imageBytes != null) {
                    out.write(imageBytes);
                } else {
                    // TODO: add error handle here
                }
                out.flush();
            } catch (Exception e) {
            }
        }
    }

    // TODO: move ConversionUtils.java to //ui/android/ to use ConversionUtils.BYTES_PER_KILOBYTE
    private static final int BYTES_PER_KILOBYTE = 1024;
    private static final String[] COLUMNS = {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
    private static final String URI_AUTHORITY_SUFFIX = ".DropDataProvider";
    private static final Object LOCK = new Object();

    private int mClearCachedDataIntervalMs = DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS;

    private byte @Nullable [] mImageBytes;

    private @Nullable String mImageFilename;
    private @Nullable String mMimeType;

    /** The URI handled by this content provider. */
    private @Nullable Uri mContentProviderUri;

    private @Nullable Handler mHandler;
    private long mDragEndTime;
    private long mOpenFileLastAccessTime;
    private @Nullable Uri mLastUri;
    private long mLastUriClearedTimestamp;
    private long mLastUriCreatedTimestamp;
    private boolean mLastUriRecorded;
    private final DropPipeDataWriter mDropPipeDataWriter = new DropPipeDataWriter();

    /** Update the delayed time before clearing the image cache. */
    public void setClearCachedDataIntervalMs(int milliseconds) {
        synchronized (LOCK) {
            mClearCachedDataIntervalMs = milliseconds;
        }
    }

    private Uri generateUri() {
        String timestamp = String.valueOf(System.currentTimeMillis());
        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_CONTENT)
                .authority(
                        ContextUtils.getApplicationContext().getPackageName()
                                + URI_AUTHORITY_SUFFIX)
                .path(timestamp)
                .build();
    }

    /**
     * Cache the passed-in image data of Drag and Drop. It is expected for filename to be non-empty.
     */
    public Uri cache(byte[] imageBytes, String encodingFormat, String filename) {
        long elapsedRealtime = SystemClock.elapsedRealtime();
        long lastUriCreatedTimestamp = mLastUriCreatedTimestamp;
        String mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(encodingFormat);
        Uri newUri = generateUri();

        synchronized (LOCK) {
            // Clear out any old data.
            clearCacheData();
            // Set new data.
            mLastUriCreatedTimestamp = elapsedRealtime;
            this.mImageBytes = imageBytes;
            mImageFilename = filename;
            mMimeType = mimeType;
            mDragEndTime = 0;
            mOpenFileLastAccessTime = 0;
            mContentProviderUri = newUri;
        }

        if (lastUriCreatedTimestamp > 0) {
            long duration = elapsedRealtime - lastUriCreatedTimestamp;
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Android.DragDrop.Image.UriCreatedInterval", duration);
        }
        int sizeInKB = imageBytes.length / BYTES_PER_KILOBYTE;
        RecordHistogram.recordCustomCountHistogram(
                "Android.DragDrop.Image.Size", sizeInKB, 1, 100_000, 50);
        return newUri;
    }

    /**
     * Clear the image data of Drag and Drop when event ACTION_DRAG_ENDED is received.
     *
     * @param imageInUse Indicate if the image is needed by the drop target app. This is true when
     *     the image is dropped outside of Chrome AND the drop target app returns true for event
     *     ACTION_DROP.
     */
    public void onDragEnd(boolean imageInUse) {
        if (!imageInUse) {
            // Clear the image data immediately when the drop target app rejects the data.
            clearCache();
        } else {
            // Otherwise, clear it with a delay to allow asynchronous data transfer.
            synchronized (LOCK) {
                clearCacheWithDelay();
                mDragEndTime = SystemClock.elapsedRealtime();
            }
        }
    }

    /** Clear the image data of Drag and Drop and record histogram. */
    void clearCache() {
        synchronized (LOCK) {
            clearCacheData();
            if (mDragEndTime > 0 && mOpenFileLastAccessTime > 0) {
                // If ContentProvider#openFile is received before Android Drag End event, set the
                // duration to 0 to avoid negative value.
                long duration = Math.max(0, mOpenFileLastAccessTime - mDragEndTime);
                RecordHistogram.deprecatedRecordMediumTimesHistogram(
                        "Android.DragDrop.Image.OpenFileTime.LastAttempt", duration);
            }
        }
    }

    /** Clear the image data of Drag and Drop. */
    private void clearCacheData() {
        mImageBytes = null;
        mImageFilename = null;
        mMimeType = null;
        if (mContentProviderUri != null) {
            mLastUri = mContentProviderUri;
            mLastUriClearedTimestamp = SystemClock.elapsedRealtime();
            mLastUriRecorded = false;
        }
        mContentProviderUri = null;
        if (mHandler != null) {
            mHandler.removeCallbacksAndMessages(null);
            mHandler = null;
        }
    }

    /** Clear the image data of Drag and Drop with delay. */
    private void clearCacheWithDelay() {
        if (mHandler == null) {
            mHandler = new Handler(Looper.getMainLooper());
        }
        mHandler.postDelayed(this::clearCache, mClearCachedDataIntervalMs);
    }

    /** A static initializer for the class. */
    @UsedByReflection("DropDataContentProvider")
    public static DropDataProviderImpl onCreate() {
        // TODO(crbug.com/40825314): Lazily create DropPipeDataWriter in #openFile.
        return new DropDataProviderImpl();
    }

    /**
     * @see ContentProvider#getType(Uri)
     */
    public @Nullable String getType(@Nullable Uri uri) {
        synchronized (LOCK) {
            if (uri == null || !uri.equals(mContentProviderUri)) {
                return null;
            }
            return mMimeType;
        }
    }

    /**
     * @see ContentProvider#getStreamTypes(Uri, String)
     */
    public String @Nullable [] getStreamTypes(Uri uri, String mimeTypeFilter) {
        String mimeType;
        synchronized (LOCK) {
            if (uri == null || !uri.equals(mContentProviderUri)) {
                return null;
            }
            mimeType = mMimeType;
        }
        return matchMimeType(mimeType, mimeTypeFilter) ? new String[] {mimeType} : null;
    }

    private boolean matchMimeType(@Nullable String mimeType, String mimeTypeFilter) {
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

    /**
     * @see ContentProvider#openAssetFile(Uri, String)
     */
    public @Nullable AssetFileDescriptor openAssetFile(
            ContentProvider providerWrapper, Uri uri, String mode)
            throws FileNotFoundException, SecurityException {
        if (uri == null) {
            return null;
        }
        byte[] imageBytes;
        long elapsedRealtime = SystemClock.elapsedRealtime();
        synchronized (LOCK) {
            if (!uri.equals(mContentProviderUri)) {
                if (uri.equals(mLastUri) && !mLastUriRecorded) {
                    long duration = elapsedRealtime - mLastUriClearedTimestamp;
                    RecordHistogram.deprecatedRecordMediumTimesHistogram(
                            "Android.DragDrop.Image.OpenFileTime.FirstExpired", duration);
                    mLastUriRecorded = true;
                }
                return null;
            }
            if (mOpenFileLastAccessTime == 0) {
                // If Android Drag End event has not been received yet, treat the duration as 0 ms.
                long duration = mDragEndTime == 0 ? 0 : elapsedRealtime - mDragEndTime;
                RecordHistogram.deprecatedRecordMediumTimesHistogram(
                        "Android.DragDrop.Image.OpenFileTime.FirstAttempt", duration);
            }
            mOpenFileLastAccessTime = elapsedRealtime;
            imageBytes = assumeNonNull(this.mImageBytes);
        }
        String type = getType(uri);
        assert type != null;
        ParcelFileDescriptor fd =
                providerWrapper.openPipeHelper(uri, type, null, imageBytes, mDropPipeDataWriter);
        return new AssetFileDescriptor(fd, 0, imageBytes.length);
    }

    /**
     * @see ContentProvider#openFile(Uri, String)
     */
    public @Nullable ParcelFileDescriptor openFile(ContentProvider providerWrapper, Uri uri)
            throws FileNotFoundException {
        AssetFileDescriptor afd = openAssetFile(providerWrapper, uri, "r");
        return afd != null ? afd.getParcelFileDescriptor() : null;
    }

    /**
     * @see ContentProvider#query(Uri, String[], String, String[], String)
     */
    public Cursor query(Uri uri, String @Nullable [] projection) {
        byte[] imageBytes;
        String imageFilename;
        synchronized (LOCK) {
            if (uri == null || !uri.equals(mContentProviderUri)) {
                return new MatrixCursor(COLUMNS, 0);
            }
            imageBytes = assumeNonNull(mImageBytes);
            imageFilename = assumeNonNull(mImageFilename);
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
            values[index] = imageFilename;
            index++;
        }
        if (hasSize) {
            cols[index] = OpenableColumns.SIZE;
            values[index] = imageBytes.length;
        }
        MatrixCursor cursor = new MatrixCursor(cols, 1);
        cursor.addRow(values);
        return cursor;
    }

    /**
     * @see ContentProvider#call(String, String, Bundle)
     */
    public @Nullable Bundle call(String method, @Nullable String arg, @Nullable Bundle extras) {
        switch (method) {
            case CACHE_METHOD_NAME:
                assumeNonNull(extras);
                byte[] imageBytes = (byte[]) extras.getSerializable(BYTES_PARAM);
                String encodingFormat = extras.getString(IMAGE_CONTENT_EXTENSION_PARAM);
                String filename = extras.getString(IMAGE_FILE_PARAM);
                assert imageBytes != null;
                assert encodingFormat != null;
                assert filename != null;
                Uri uri = cache(imageBytes, encodingFormat, filename);
                Bundle bundleToReturn = new Bundle();
                bundleToReturn.putParcelable("uri", uri);
                return bundleToReturn;
            case SET_INTERVAL_METHOD_NAME:
                assumeNonNull(extras);
                setClearCachedDataIntervalMs(
                        extras.getInt(
                                CLEAR_CACHE_PARAM,
                                DropDataProviderImpl.DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS));
                break;
            case ON_DRAG_END_METHOD_NAME:
                assumeNonNull(extras);
                onDragEnd(extras.getBoolean(IMAGE_USAGE_PARAM));
                break;
        }

        return null;
    }

    byte @Nullable [] getImageBytesForTesting() {
        synchronized (LOCK) {
            return mImageBytes;
        }
    }

    @Nullable
    Handler getHandlerForTesting() {
        synchronized (LOCK) {
            return mHandler;
        }
    }

    void clearLastUriCreatedTimestampForTesting() {
        synchronized (LOCK) {
            mLastUriCreatedTimestamp = 0;
        }
    }
}
