// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

@JNINamespace("wolvic")
public class FileSelectManager {
    public interface Bridge {
        void selectFile(String[] acceptMimeTypes, boolean capture, boolean allowMultiple);
        void close();
    }

    public interface Listener {
        void onFileSelected(String filePath);
        void onMultipleFilesSelected(String[] filePathArray);
        void onCanceled();
    }

    public interface Factory {
        public Bridge create(Listener listener);
    }

    private static Factory sFactory;
    private final Bridge mBridge;
    private long mNativeFileSelectManagerHandle;

    private FileSelectManager(long nativeFileSelectManagerHandle) {
        Listener listener = new Listener() {
            @Override
            public void onFileSelected(String filePath) {
                FileSelectManagerJni.get().onFileSelected(
                        mNativeFileSelectManagerHandle, filePath);
            }

            @Override
            public void onMultipleFilesSelected(String[] filePathArray) {
                FileSelectManagerJni.get().onMultipleFilesSelected(
                        mNativeFileSelectManagerHandle, filePathArray);
            }

            @Override
            public void onCanceled() {
                FileSelectManagerJni.get().onFileSelectionCanceled(mNativeFileSelectManagerHandle);
            }
        };

        mNativeFileSelectManagerHandle = nativeFileSelectManagerHandle;
        mBridge = sFactory.create(listener);
    }

    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    @CalledByNative
    public static FileSelectManager create(long nativeFileSelectManagerHandle) {
        return new FileSelectManager(nativeFileSelectManagerHandle);
    }

    @CalledByNative
    void selectFile(String[] acceptMimeTypes, boolean capture, boolean allowMultiple) {
        mBridge.selectFile(acceptMimeTypes, capture, allowMultiple);
    }

    @CalledByNative
    void destroyed() {
        mNativeFileSelectManagerHandle = 0;
    }

    @NativeMethods
    interface Natives {
        void onFileSelected(long nativeFileSelectManager, String filePath);
        void onMultipleFilesSelected(long nativeFileSelectManager, String[] filePathArray);
        void onFileSelectionCanceled(long nativeFileSelectManager);
    }
}
