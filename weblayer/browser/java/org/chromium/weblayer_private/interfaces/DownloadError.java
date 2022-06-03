// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({DownloadError.NO_ERROR, DownloadError.SERVER_ERROR, DownloadError.SSL_ERROR,
        DownloadError.CONNECTIVITY_ERROR, DownloadError.NO_SPACE, DownloadError.FILE_ERROR,
        DownloadError.CANCELLED, DownloadError.OTHER_ERROR})
@Retention(RetentionPolicy.SOURCE)
public @interface DownloadError {
    int NO_ERROR = 0;
    int SERVER_ERROR = 1;
    int SSL_ERROR = 2;
    int CONNECTIVITY_ERROR = 3;
    int NO_SPACE = 4;
    int FILE_ERROR = 5;
    int CANCELLED = 6;
    int OTHER_ERROR = 7;
}
