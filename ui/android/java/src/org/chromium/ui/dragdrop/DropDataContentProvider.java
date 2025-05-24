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

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.FileNotFoundException;

/**
 * ContentProvider for Chrome image data of Drag and Drop. A light wrapper around {@link
 * DropDataProviderImpl}.
 *
 * The content provider acts a wrapper over the core implementation in order to be able to access
 * the feature from different platforms as other platform won't be able to access the impl
 * class directly as it lives in other classloader than the app's one (Chromium classloader).
 */
@NullMarked
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
    public @Nullable String getType(Uri uri) {
        return mDropDataProviderImpl.getType(uri);
    }

    @Override
    public String @Nullable [] getStreamTypes(Uri uri, String mimeTypeFilter) {
        return mDropDataProviderImpl.getStreamTypes(uri, mimeTypeFilter);
    }

    @Override
    public @Nullable AssetFileDescriptor openAssetFile(Uri uri, String mode)
            throws FileNotFoundException, SecurityException {
        return mDropDataProviderImpl.openAssetFile(this, uri, mode);
    }

    @Override
    public @Nullable ParcelFileDescriptor openFile(Uri uri, String mode)
            throws FileNotFoundException {
        return mDropDataProviderImpl.openFile(this, uri);
    }

    @Override
    public Cursor query(
            Uri uri,
            String @Nullable [] projection,
            @Nullable String selection,
            String @Nullable [] selectionArgs,
            @Nullable String sortOrder) {
        return mDropDataProviderImpl.query(uri, projection);
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

    @Override
    public Uri insert(Uri uri, @Nullable ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public @Nullable Bundle call(String method, @Nullable String arg, @Nullable Bundle extras) {
        return mDropDataProviderImpl.call(method, arg, extras);
    }

    @VisibleForTesting
    public void setDropDataProviderImpl(DropDataProviderImpl dropDataProviderImpl) {
        mDropDataProviderImpl = dropDataProviderImpl;
    }
}
