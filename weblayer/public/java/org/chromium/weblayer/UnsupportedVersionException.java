// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Error thrown if client and implementation versions are not compatible.
 */
class UnsupportedVersionException extends RuntimeException {
    /**
     * Constructs a new exception with the specified version.
     */
    public UnsupportedVersionException(String implementationVersion) {
        super("Unsupported WebLayer version, client version "
                + WebLayerClientVersionConstants.PRODUCT_VERSION
                + " is not supported by implementation version " + implementationVersion);
    }
}
