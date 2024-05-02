// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import org.chromium.base.test.util.Restriction;

/**
 * GmsCoreVersionRestriction list form factor restrictions, that are usable with the {@link
 * Restriction} annotation in layers depending on //ui. E.g. <code>
 *   \@Restriction({GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W24})
 * </code>
 */
public final class GmsCoreVersionRestriction {
    /** Specifies the test to run only with the GMS Core version greater or equal 22w30. */
    public static final String RESTRICTION_TYPE_VERSION_GE_22W30 = "GMSCoreVersion22w30";
}
