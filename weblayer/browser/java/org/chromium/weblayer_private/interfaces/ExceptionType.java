// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ExceptionType.RESTRICTED_API, ExceptionType.UNKNOWN})
@Retention(RetentionPolicy.SOURCE)
public @interface ExceptionType {
    int UNKNOWN = 0;
    int RESTRICTED_API = 1;
}
