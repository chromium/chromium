// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import android.graphics.Bitmap;
import android.view.View;

public class SampleNonProxy {
    interface OnFrameAvailableListener {}
    private native int nativeInit();
    private native void nativeDestroy(int nativeChromeBrowserProvider);
    private native long nativeAddBookmark(int nativeChromeBrowserProvider, String url, String title,
            boolean isFolder, long parentId);
    private static native String nativeGetDomainAndRegistry(String url);
    private static native void nativeCreateHistoricalTabFromState(byte[] state, int tabIndex);
    private native byte[] nativeGetStateAsByteArray(View view);
    private static native String[] nativeGetAutofillProfileGUIDs();
    private native void nativeSetRecognitionResults(int sessionId, String[] results);
    private native long nativeAddBookmarkFromAPI(int nativeChromeBrowserProvider, String url,
            Long created, Boolean isBookmark, Long date, byte[] favicon, String title,
            Integer visits);
    native int nativeFindAll(String find);
    private static native OnFrameAvailableListener nativeGetInnerClass();
    private native Bitmap nativeQueryBitmap(int nativeChromeBrowserProvider, String[] projection,
            String selection, String[] selectionArgs, String sortOrder);
    private native void nativeGotOrientation(
            int nativeDataFetcherImplAndroid, double alpha, double beta, double gamma);
    private static native Throwable nativeMessWithJavaException(Throwable e);
    class MyInnerClass {
        private native int nativeInit();
    }
}
