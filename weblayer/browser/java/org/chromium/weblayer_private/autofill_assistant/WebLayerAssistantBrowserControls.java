
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.autofill_assistant;

import org.chromium.components.autofill_assistant.AssistantBrowserControls;

/**
 * Implementation of {@link AssistantBrowserControls} for WebLayer.
 */
public class WebLayerAssistantBrowserControls implements AssistantBrowserControls {
    private AssistantBrowserControls.Observer mObserver;

    @Override
    public int getBottomControlsHeight() {
        // TODO(b/222671580): Implement
        return 0;
    }

    @Override
    public int getBottomControlOffset() {
        // TODO(b/222671580): Implement
        return 0;
    }

    @Override
    public int getContentOffset() {
        // TODO(b/222671580): Implement
        return 0;
    }

    @Override
    public float getTopVisibleContentOffset() {
        // TODO(b/222671580): Implement
        return 0;
    }

    @Override
    public void setObserver(AssistantBrowserControls.Observer browserControlsObserver) {
        mObserver = browserControlsObserver;
    }

    @Override
    public void destroy() {}

    public void onControlsOffsetChanged() {
        mObserver.onControlsOffsetChanged();
    }

    public void onBottomControlsHeightChanged() {
        mObserver.onBottomControlsHeightChanged();
    }
}
