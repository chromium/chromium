// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ExceptionType.RESTRICTED_API, ExceptionType.INVALID_ARGUMENT, ExceptionType.UNKNOWN})
@Retention(RetentionPolicy.SOURCE)
public @interface ExceptionType {
    int RESTRICTED_API = 0;
    int INVALID_ARGUMENT = 1;
    int UNKNOWN = 2;
}
