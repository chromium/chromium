// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({BrowserEmbeddabilityMode.UNSUPPORTED, BrowserEmbeddabilityMode.SUPPORTED,
        BrowserEmbeddabilityMode.SUPPORTED_WITH_TRANSPARENT_BACKGROUND})
@Retention(RetentionPolicy.SOURCE)
public @interface BrowserEmbeddabilityMode {
    /**
     * Does not support embedding use case.
     * The underlying implementation for this mode should remain identical to chrome.
     * This mode is ideal for displaying general web content and is the most optimized
     * for efficiency and performance.
     * It has no support for embedding use cases and should generally be made to occupy
     * the whole window without being moved. In particular, this mode does not support
     * applying View transforms or transparency. Also app should have at most one Browser
     * instance in this mode per window.
     */
    int UNSUPPORTED = org.chromium.weblayer_private.interfaces.BrowserEmbeddabilityMode.UNSUPPORTED;

    /**
     * Supports embedding use cases.
     * The underlying implementation is less efficient and less performant than UNSUPPORTED
     * to enable support full support for general View behaviors. Weblayer in this mode
     * fully supports being moved on screen, or have View transform or transparency applied.
     * It is suitable for displaying general web content, though some situations may be
     * significantly less efficient.
     */
    int SUPPORTED = org.chromium.weblayer_private.interfaces.BrowserEmbeddabilityMode.SUPPORTED;

    /**
     * Same as SUPPORTED with the page background set to transparent.
     * This allows the page's background color (with transparency) to blend with Views below
     * WebLayer. Note it is the embedder's responsibility to ensure there is no graphical
     * corruption. This is web-breaking change, so is only suitable for displaying web content
     * fully controlled by the embedder, not for general web content.
     */
    int SUPPORTED_WITH_TRANSPARENT_BACKGROUND =
            org.chromium.weblayer_private.interfaces.BrowserEmbeddabilityMode
                    .SUPPORTED_WITH_TRANSPARENT_BACKGROUND;
}
