// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * Error thrown if there is an API access violation due to 1-party restriction.
 */
public class RestrictedAPIException extends RuntimeException {
    public static final String MESSAGE = "Method restricted to verified origin";

    public static boolean isInstance(RuntimeException e) {
        return MESSAGE.equals(e.getMessage());
    }

    public RestrictedAPIException() {
        super(MESSAGE);
    }
}
