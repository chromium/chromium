// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;

@JNINamespace("wolvic")
public class Tab {
    private ActivityWindowAndroid mWindowAndroid;
    private ContentView mContentView;
    private NavigationController mNavigationController;
    private TabCompositorView mCompositorView;
    protected WebContents mWebContents;

    public WebContents createWebContents(boolean is_off_the_record) {
        return TabJni.get().createWebContents(is_off_the_record);
    }

    public void setWebContentsDelegate(
            WebContents webContents, WebContentsDelegateAndroid delegate) {
        TabJni.get().setWebContentsDelegate(webContents, delegate);
    }

    public Tab(@NonNull Context context, boolean is_off_the_record) {
        mWindowAndroid = new ActivityWindowAndroid(context, false,
                IntentRequestTracker.createFromActivity(ContextUtils.activityFromContext(context)));

        mCompositorView = new TabCompositorView(context);
        mCompositorView.onNativeLibraryLoaded(mWindowAndroid);
        mWindowAndroid.setAnimationPlaceholderView(mCompositorView);

        mWebContents = createWebContents(is_off_the_record);

        mContentView =
                ContentView.createContentView(context, null /* eventOffsetHandler */, mWebContents);
        mWebContents.initialize("", ViewAndroidDelegate.createBasicDelegate(mContentView),
                mContentView, mWindowAndroid, WebContents.createDefaultInternalsHolder());

        mNavigationController = mWebContents.getNavigationController();

        mCompositorView.setCurrentWebContents(mWebContents);

        // TODO: Call `onShow()` on the appropriate place and should be pair
        // with `onHide()`.
        mWebContents.onShow();
    }

    public void goBack() {
        mNavigationController.goBack();
    }

    public void goForward() {
        mNavigationController.goForward();
    }

    public void reload() {
        mNavigationController.reload(true);
    }

    public void loadUrl(@NonNull String uri) {
        LoadUrlParams params =
                new LoadUrlParams(UrlFormatter.fixupUrl(uri).getPossiblyInvalidSpec());
        mNavigationController.loadUrl(params);
    }

    public TabCompositorView getCompositorView() {
        return mCompositorView;
    }

    public ContentView getContentView() {
        return mContentView;
    }

    public ImeAdapter getImeAdapter() {
        return ImeAdapter.fromWebContents(mWebContents);
    }

    @NativeMethods
    public interface Natives {
        WebContents createWebContents(boolean is_off_the_record);
        void setWebContentsDelegate(WebContents webContents, WebContentsDelegateAndroid delegate);
    }
}
