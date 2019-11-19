// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({LoadError.NO_ERROR, LoadError.HTTP_CLIENT_ERROR, LoadError.HTTP_SERVER_ERROR,
        LoadError.SSL_ERROR, LoadError.CONNECTIVITY_ERROR, LoadError.OTHER_ERROR})
@Retention(RetentionPolicy.SOURCE)
public @interface LoadError {
    /**
     * Navigation completed successfully.
     */
    int NO_ERROR = org.chromium.weblayer_private.interfaces.LoadError.NO_ERROR;

    /**
     * Server responded with 4xx status code.
     */
    int HTTP_CLIENT_ERROR = org.chromium.weblayer_private.interfaces.LoadError.HTTP_CLIENT_ERROR;
    /**
     * Server responded with 5xx status code.
     */
    int HTTP_SERVER_ERROR = org.chromium.weblayer_private.interfaces.LoadError.HTTP_SERVER_ERROR;

    /**
     * Certificate error.
     */
    int SSL_ERROR = org.chromium.weblayer_private.interfaces.LoadError.SSL_ERROR;

    /**
     * Problem connecting to server.
     */
    int CONNECTIVITY_ERROR = org.chromium.weblayer_private.interfaces.LoadError.CONNECTIVITY_ERROR;

    /**
     * An error not listed above occurred.
     */
    int OTHER_ERROR = org.chromium.weblayer_private.interfaces.LoadError.OTHER_ERROR;
}
