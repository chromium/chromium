// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({BrowserEmbeddabilityMode.UNSUPPORTED, BrowserEmbeddabilityMode.SUPPORTED,
        BrowserEmbeddabilityMode.SUPPORTED_WITH_TRANSPARENT_BACKGROUND})
@Retention(RetentionPolicy.SOURCE)
public @interface BrowserEmbeddabilityMode {
    int UNSUPPORTED = 0;
    int SUPPORTED = 1;
    int SUPPORTED_WITH_TRANSPARENT_BACKGROUND = 2;
}
