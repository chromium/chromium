// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({DownloadState.IN_PROGRESS, DownloadState.COMPLETE, DownloadState.PAUSED,
        DownloadState.CANCELLED, DownloadState.FAILED})
@Retention(RetentionPolicy.SOURCE)
@interface DownloadState {
    int IN_PROGRESS = org.chromium.weblayer_private.interfaces.DownloadState.IN_PROGRESS;
    int COMPLETE = org.chromium.weblayer_private.interfaces.DownloadState.COMPLETE;
    int PAUSED = org.chromium.weblayer_private.interfaces.DownloadState.PAUSED;
    int CANCELLED = org.chromium.weblayer_private.interfaces.DownloadState.CANCELLED;
    int FAILED = org.chromium.weblayer_private.interfaces.DownloadState.FAILED;
}
