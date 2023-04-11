// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import android.content.Context;
import android.graphics.PixelFormat;
import android.view.Surface;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.components.embedder_support.view.ContentViewRenderViewJni;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the chromium compositor and the Surface that is used by
 * the chromium compositor. Note it can be used to display only one WebContents.
 * TODO: Need to implement own compisitor client for wolvic which inherits
 * `content::CompositorClient`.
 */
public class TabCompositorView extends ContentViewRenderView {
    private Surface mSurface;

    public TabCompositorView(Context context) {
        super(context);
    }

    @Override
    public void onNativeLibraryLoaded(@NonNull WindowAndroid rootWindow) {
        mNativeContentViewRenderView = ContentViewRenderViewJni.get().init(this, rootWindow);
        assert mNativeContentViewRenderView != 0;
        mWindowAndroid = rootWindow;
    }

    public void surfaceChanged(Surface surface, int width, int height) {
        assert mNativeContentViewRenderView != 0;
        if (mSurface != surface) {
            // Consider this condition as new surface creation.
            ContentViewRenderViewJni.get().surfaceCreated(mNativeContentViewRenderView, this);
        }
        mSurface = surface;

        try {
            ContentViewRenderViewJni.get().surfaceChanged(
                    mNativeContentViewRenderView, this, PixelFormat.OPAQUE, width, height, surface);
            if (mWebContents != null) {
                ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                        mNativeContentViewRenderView, this, mWebContents, width, height);
            }
            setViewSize(width, height);
        } catch (Exception ex) {
            throw new RuntimeException(ex);
        }
    }

    public void surfaceDestroyed() {
        if (mSurface == null) {
            return;
        }

        assert mNativeContentViewRenderView != 0;
        ContentViewRenderViewJni.get().surfaceDestroyed(mNativeContentViewRenderView, this);
        mSurface = null;
    }

    public void insertVisualStateCallback(Callback<Boolean> callback) {
        assert mWebContents != null;
        mWebContents.getMainFrame().insertVisualStateCallback(callback);
    }

    private void setViewSize(int width, int height) {
        setLayoutParams(new LayoutParams(width, height));
    }
}
