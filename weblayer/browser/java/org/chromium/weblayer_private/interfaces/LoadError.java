// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({LoadError.NO_ERROR, LoadError.HTTP_CLIENT_ERROR, LoadError.HTTP_SERVER_ERROR,
        LoadError.SSL_ERROR, LoadError.CONNECTIVITY_ERROR, LoadError.OTHER_ERROR})
@Retention(RetentionPolicy.SOURCE)
public @interface LoadError {
    int NO_ERROR = 0;
    int HTTP_CLIENT_ERROR = 1;
    int HTTP_SERVER_ERROR = 2;
    int SSL_ERROR = 3;
    int CONNECTIVITY_ERROR = 4;
    int OTHER_ERROR = 5;
}
