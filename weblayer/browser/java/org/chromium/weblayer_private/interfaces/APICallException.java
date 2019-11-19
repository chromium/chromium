// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.util.AndroidRuntimeException;

/**
 * Error thrown if there is an error communicating over the AIDL boundary.
 */
public class APICallException extends AndroidRuntimeException {
    /**
     * Constructs a new exception with the specified cause.
     */
    public APICallException(Exception cause) {
        super(cause);
    }
}
