// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.components.webxr.ArCompositorDelegate;
import org.chromium.content_public.browser.WebContents;

/**
 * Weblayer-specific implementation of ArCompositorDelegate interface.
 */
public class ArCompositorDelegateImpl implements ArCompositorDelegate {
    private final BrowserImpl mBrowser;

    ArCompositorDelegateImpl(WebContents webContents) {
        TabImpl tab = TabImpl.fromWebContents(webContents);
        mBrowser = tab.getBrowser();
    }

    @Override
    public void setOverlayImmersiveArMode(boolean enabled, boolean domSurfaceNeedsConfiguring) {
        BrowserViewController controller =
                mBrowser.getBrowserFragment().getPossiblyNullViewController();
        if (controller != null) {
            controller.setSurfaceProperties(/*requiresAlphaChannel=*/enabled,
                    /*zOrderMediaOverlay=*/domSurfaceNeedsConfiguring);
        }
    }

    @Override
    public void dispatchTouchEvent(MotionEvent ev) {
        BrowserViewController controller =
                mBrowser.getBrowserFragment().getPossiblyNullViewController();
        if (controller != null) {
            controller.getContentView().dispatchTouchEvent(ev);
        }
    }

    @Override
    @NonNull
    public ViewGroup getArSurfaceParent() {
        BrowserViewController controller =
                mBrowser.getBrowserFragment().getPossiblyNullViewController();
        if (controller == null) return null;
        return controller.getArViewHolder();
    }

    @Override
    public boolean shouldToggleArSurfaceParentVisibility() {
        return true;
    }
}
