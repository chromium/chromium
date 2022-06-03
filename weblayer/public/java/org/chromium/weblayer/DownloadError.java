// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({DownloadError.NO_ERROR, DownloadError.SERVER_ERROR, DownloadError.SSL_ERROR,
        DownloadError.CONNECTIVITY_ERROR, DownloadError.NO_SPACE, DownloadError.FILE_ERROR,
        DownloadError.CANCELLED, DownloadError.OTHER_ERROR})
@Retention(RetentionPolicy.SOURCE)
public @interface DownloadError {
    int NO_ERROR = org.chromium.weblayer_private.interfaces.DownloadError.NO_ERROR;
    int SERVER_ERROR = org.chromium.weblayer_private.interfaces.DownloadError.SERVER_ERROR;
    int SSL_ERROR = org.chromium.weblayer_private.interfaces.DownloadError.SSL_ERROR;
    int CONNECTIVITY_ERROR =
            org.chromium.weblayer_private.interfaces.DownloadError.CONNECTIVITY_ERROR;
    int NO_SPACE = org.chromium.weblayer_private.interfaces.DownloadError.NO_SPACE;
    int FILE_ERROR = org.chromium.weblayer_private.interfaces.DownloadError.FILE_ERROR;
    int CANCELLED = org.chromium.weblayer_private.interfaces.DownloadError.CANCELLED;
    int OTHER_ERROR = org.chromium.weblayer_private.interfaces.DownloadError.OTHER_ERROR;
}
