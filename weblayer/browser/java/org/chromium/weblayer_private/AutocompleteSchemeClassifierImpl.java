// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;

/**
 * Creates the c++ class that provides scheme classification logic for WebLayer
 * Must call destroy() after using this object to delete the native object.
 */
@JNINamespace("weblayer")
public class AutocompleteSchemeClassifierImpl extends AutocompleteSchemeClassifier {
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public AutocompleteSchemeClassifierImpl() {
        super(AutocompleteSchemeClassifierImplJni.get().createAutocompleteClassifier());
    }

    @Override
    public void destroy() {
        super.destroy();

        AutocompleteSchemeClassifierImplJni.get().deleteAutocompleteClassifier(
                super.getNativePtr());

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    @NativeMethods
    interface Natives {
        long createAutocompleteClassifier();
        void deleteAutocompleteClassifier(long weblayerAutocompleteSchemeClassifier);
    }
}
