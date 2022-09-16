// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * Versioning related constants.
 */
public interface WebLayerVersionConstants {
    /**
     * Maximum allowed version skew. If the skew is greater than this, the implementation and client
     * are not considered compatible, and WebLayer is unusable. The skew is the absolute value of
     * the difference between the client major version and the implementation major version.
     *
     * @see WebLayer#isAvailable()
     */
    int MAX_SKEW = 9;

    /**
     * Minimum version of client and implementation.
     */
    int MIN_VERSION = 87;
}
