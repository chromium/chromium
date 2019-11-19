// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Error thrown if client and implementation versions are not compatible.
 */
public class UnsupportedVersionException extends Exception {
    /**
     * Constructs a new exception with the specified version.
     */
    public UnsupportedVersionException(int clientVersion) {
        super("Unsupported WebLayer version, client version " + clientVersion
                + " is not supported by the implementation.");
    }
}
