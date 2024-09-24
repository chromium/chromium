// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import org.chromium.ui.base.DeviceFormFactor;

/**
 * Disable if enums that are usable with the @DisableIf in layers depending on //ui.
 *
 * <p>e.g. @DisableIf.Device(DeviceFormFactors.PHONE)
 *
 * <p>TODO(crbug.com/368410229): Update references to use FormFactors instead.
 */
public final class UiDisableIf {
    public static final String PHONE = DeviceFormFactor.PHONE;
    public static final String TABLET = DeviceFormFactor.TABLET;
}
