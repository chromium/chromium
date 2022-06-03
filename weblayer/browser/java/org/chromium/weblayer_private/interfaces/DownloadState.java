// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({DownloadState.IN_PROGRESS, DownloadState.COMPLETE, DownloadState.PAUSED,
        DownloadState.CANCELLED, DownloadState.FAILED})
@Retention(RetentionPolicy.SOURCE)
public @interface DownloadState {
    int IN_PROGRESS = 0;
    int COMPLETE = 1;
    int PAUSED = 2;
    int CANCELLED = 3;
    int FAILED = 4;
}
