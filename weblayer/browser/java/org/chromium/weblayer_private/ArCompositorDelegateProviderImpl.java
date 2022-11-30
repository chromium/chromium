// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.webxr.ArCompositorDelegate;
import org.chromium.components.webxr.ArCompositorDelegateProvider;
import org.chromium.content_public.browser.WebContents;

/**
 * Weblayer-specific implementation of ArCompositorDelegateProvider interface.
 */
@JNINamespace("weblayer")
public class ArCompositorDelegateProviderImpl implements ArCompositorDelegateProvider {
    @CalledByNative
    public ArCompositorDelegateProviderImpl() {}

    @Override
    public ArCompositorDelegate create(WebContents webContents) {
        return new ArCompositorDelegateImpl(webContents);
    }
}
