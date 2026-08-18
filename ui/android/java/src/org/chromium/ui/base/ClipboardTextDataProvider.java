// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.chromium.ui.base.MimeTypeUtils.TEXT_HTML_MIME_TYPE;
import static org.chromium.ui.base.MimeTypeUtils.TEXT_PLAIN_MIME_TYPE;

import android.content.ClipDescription;
import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

/**
 * Serves clipboard text/plain and text/html payloads too large to fit inline in a {@link
 * android.content.ClipData}. Does not expose {@link android.provider.OpenableColumns}, so {@code
 * ContentUriUtils#isOpenableFile} reports false and readers treat the URI as a stream to paste
 * rather than a file to attach.
 *
 * <p>Uses a capability token query parameter (?uuid=...) and in-memory pipe streaming via {@link
 * ContentProvider#openPipeHelper} to serve the data.
 */
@NullMarked
public class ClipboardTextDataProvider extends ContentProvider {
    private static final String AUTHORITY_SUFFIX = ".ClipboardTextDataProvider";
    private static final String QUERY_PARAM_UUID = "uuid";
    private static final Object sLock = new Object();

    private static @Nullable String sCurrentUuid;
    private static byte @Nullable [] sTextBytes;
    private static byte @Nullable [] sHtmlBytes;

    /** Stages |text| and/or |html| in memory and returns a content:// URI, or null on failure. */
    /* package */ static @Nullable Uri store(@Nullable String text, @Nullable String html) {
        if (text == null && html == null) return null;

        String newUuid = UUID.randomUUID().toString();
        byte[] textBytes = text != null ? text.getBytes(StandardCharsets.UTF_8) : null;
        byte[] htmlBytes = html != null ? html.getBytes(StandardCharsets.UTF_8) : null;

        synchronized (sLock) {
            sCurrentUuid = newUuid;
            sTextBytes = textBytes;
            sHtmlBytes = htmlBytes;
        }

        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_CONTENT)
                .authority(ContextUtils.getApplicationContext().getPackageName() + AUTHORITY_SUFFIX)
                .appendQueryParameter(QUERY_PARAM_UUID, newUuid)
                .build();
    }

    private static boolean validateUuidQueryParam(Uri uri) {
        String requestUuid = uri.getQueryParameter(QUERY_PARAM_UUID);
        return sCurrentUuid != null && sCurrentUuid.equals(requestUuid);
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public @Nullable String getType(Uri uri) {
        synchronized (sLock) {
            if (!validateUuidQueryParam(uri)) return null;
            if (sHtmlBytes != null) return TEXT_HTML_MIME_TYPE;
            if (sTextBytes != null) return TEXT_PLAIN_MIME_TYPE;
            return null;
        }
    }

    @Override
    public String @Nullable [] getStreamTypes(Uri uri, String mimeTypeFilter) {
        synchronized (sLock) {
            if (!validateUuidQueryParam(uri)) return null;
            List<String> types = new ArrayList<>();
            if (sTextBytes != null) types.add(TEXT_PLAIN_MIME_TYPE);
            if (sHtmlBytes != null) types.add(TEXT_HTML_MIME_TYPE);
            if (types.isEmpty()) return null;
            return new ClipDescription(null, types.toArray(new String[0]))
                    .filterMimeTypes(mimeTypeFilter);
        }
    }

    @Override
    public AssetFileDescriptor openTypedAssetFile(
            Uri uri, String mimeTypeFilter, @Nullable Bundle opts) throws FileNotFoundException {
        byte[] data;
        synchronized (sLock) {
            if (!validateUuidQueryParam(uri)) {
                throw new FileNotFoundException(uri.toString());
            }

            if (sTextBytes != null
                    && ClipDescription.compareMimeTypes(TEXT_PLAIN_MIME_TYPE, mimeTypeFilter)) {
                data = sTextBytes;
            } else if (sHtmlBytes != null
                    && ClipDescription.compareMimeTypes(TEXT_HTML_MIME_TYPE, mimeTypeFilter)) {
                data = sHtmlBytes;
            } else {
                throw new FileNotFoundException(uri.toString());
            }
        }

        ParcelFileDescriptor pfd =
                openPipeHelper(
                        uri,
                        mimeTypeFilter,
                        opts,
                        data,
                        ClipboardTextDataProvider::writeDataToPipe);
        return new AssetFileDescriptor(pfd, 0, data.length);
    }

    private static void writeDataToPipe(
            ParcelFileDescriptor output,
            Uri uri,
            String mimeType,
            @Nullable Bundle opts,
            byte @Nullable [] data) {
        try (OutputStream out = new ParcelFileDescriptor.AutoCloseOutputStream(output)) {
            if (data != null) {
                out.write(data);
            }
        } catch (IOException ignored) {
        }
    }

    // ContentProvider methods below are not supported by ClipboardTextDataProvider.
    @Override
    public Cursor query(
            Uri uri,
            String @Nullable [] projection,
            @Nullable String selection,
            String @Nullable [] selectionArgs,
            @Nullable String sortOrder) {
        // Not designed to handle queries. Returns an empty cursor.
        return new MatrixCursor(new String[] {"_id"}, 0);
    }

    @Override
    public Uri insert(Uri uri, @Nullable ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int update(
            Uri uri,
            @Nullable ContentValues values,
            @Nullable String where,
            String @Nullable [] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, @Nullable String selection, String @Nullable [] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    /** Helper for tests to clear the stored data. */
    static void clearForTesting() {
        synchronized (sLock) {
            sCurrentUuid = null;
            sTextBytes = null;
            sHtmlBytes = null;
        }
    }
}
