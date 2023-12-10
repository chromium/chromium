// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

/**
 * Disable if enums that are usable with the @DisableIf in layers depending on //ui.
 *
 * e.g. @DisableIf.Device(type = {UiDisableIf.PHONE})
 */
public final class UiDisableIf {
    /** Specifies the test is disabled if on phone form factors. */
    public static final String PHONE = "Phone";

    /** Specifies the test is disabled if on tablet form factors. */
    public static final String TABLET = "Tablet";

    /** Specifies the test is disabled if on large tablet form factors. */
    public static final String LARGETABLET = "LargeTablet";
}
