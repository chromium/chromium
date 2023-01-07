// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.bluetooth;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.permissions.BluetoothScanningPromptAndroidDelegate;
import org.chromium.weblayer_private.AutocompleteSchemeClassifierImpl;

/**
 *  The implementation of {@link BluetoothScanningPromptAndroidDelegate} for WebLayer.
 */
@JNINamespace("weblayer")
public class WebLayerBluetoothScanningPromptAndroidDelegate
        implements BluetoothScanningPromptAndroidDelegate {
    /**
     * {@inheritDoc}
     */
    @Override
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return new AutocompleteSchemeClassifierImpl();
    }

    @CalledByNative
    private static WebLayerBluetoothScanningPromptAndroidDelegate create() {
        return new WebLayerBluetoothScanningPromptAndroidDelegate();
    }
}
