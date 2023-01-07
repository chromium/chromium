// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.bluetooth;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.permissions.BluetoothChooserAndroidDelegate;
import org.chromium.weblayer_private.AutocompleteSchemeClassifierImpl;

/**
 *  The implementation of {@link BluetoothChooserAndroidDelegate} for WebLayer.
 */
@JNINamespace("weblayer")
public class WebLayerBluetoothChooserAndroidDelegate implements BluetoothChooserAndroidDelegate {
    /**
     * {@inheritDoc}
     */
    @Override
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return new AutocompleteSchemeClassifierImpl();
    }

    @CalledByNative
    private static WebLayerBluetoothChooserAndroidDelegate create() {
        return new WebLayerBluetoothChooserAndroidDelegate();
    }
}
