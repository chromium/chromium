// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.io.FileNotFoundException;

/**
 * ContentProvider for Chrome image data of Drag and Drop. A light wrapper around {@link
 * DropDataProviderImpl}.
 *
 * The content provider acts a wrapper over the core implementation in order to be able to access
 * the feature from different platforms as other platform won't be able to access the impl
 * class directly as it lives in other classloader than the app's one (Chromium classloader).
 */
public class DropDataContentProvider extends ContentProvider {
    private DropDataProviderImpl mDropDataProviderImpl;

    /**
     * Initialize the core implementation and the rest of the methods will be delegation to this
     * implementation.
     */
    @Override
    public boolean onCreate() {
        // TODO(crbug.com/40825314): Lazily create DropPipeDataWriter in #openFile.
        mDropDataProviderImpl = new DropDataProviderImpl();
        return true;
    }

    @Override
    public String getType(Uri uri) {
        return mDropDataProviderImpl.getType(uri);
    }

    @Override
    public String[] getStreamTypes(Uri uri, String mimeTypeFilter) {
        return mDropDataProviderImpl.getStreamTypes(uri, mimeTypeFilter);
    }

    @Override
    public AssetFileDescriptor openAssetFile(Uri uri, String mode)
            throws FileNotFoundException, SecurityException {
        return mDropDataProviderImpl.openAssetFile(this, uri, mode);
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        return mDropDataProviderImpl.openFile(this, uri);
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        return mDropDataProviderImpl.query(uri, projection);
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

    @Nullable
    @Override
    public Bundle call(@NonNull String method, @Nullable String arg, @Nullable Bundle extras) {
        return mDropDataProviderImpl.call(method, arg, extras);
    }

    @VisibleForTesting
    public void setDropDataProviderImpl(DropDataProviderImpl dropDataProviderImpl) {
        mDropDataProviderImpl = dropDataProviderImpl;
    }
}
