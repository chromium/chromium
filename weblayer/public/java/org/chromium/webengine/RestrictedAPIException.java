// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

/**
 * Error thrown for API access violations.
 */
public class RestrictedAPIException extends RuntimeException {
    public RestrictedAPIException(String message) {
        super(message);
    }
}
