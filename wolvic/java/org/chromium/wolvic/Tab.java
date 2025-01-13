// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.wolvic.WolvicWebContentsDelegate;
import org.chromium.wolvic.WolvicWebContentsFactory;

@JNINamespace("wolvic")
public class Tab {
    // The following key and value must be set on a navigation entry extra data to let browser
    // know that the entry must be skipped when going back/forward.
    public static final String NAVIGATION_ENTRY_MARKED_AS_SKIPPED_KEY =
            "NAVIGATION_ENTRY_MARKED_AS_SKIPPED_KEY";
    public static final String NAVIGATION_ENTRY_MARKED_AS_SKIPPED_VALUE =
            "NAVIGATION_ENTRY_MARKED_AS_SKIPPED_VALUE";

    private enum NavigationDirection { BACK, FORWARD };

    private ActivityWindowAndroid mWindowAndroid;
    private ContentView mContentView;
    private NavigationController mNavigationController;
    private TabCompositorView mCompositorView;
    protected WebContents mWebContents;

    private void attachWebContents(WebContents mWebContents) {
        TabJni.get().attachWebContents(mWebContents);
    }

    private void releaseWebContents(WebContents webContents) {
        TabJni.get().releaseWebContents(webContents);
    }

    public void setWebContentsDelegate(
            WebContents webContents, WolvicWebContentsDelegate delegate) {
        TabJni.get().setWebContentsDelegate(webContents, delegate);
    }

    public Tab(@NonNull Context context, boolean is_off_the_record, WebContents webContents) {
        mWindowAndroid = new ActivityWindowAndroid(context, false,
                IntentRequestTracker.createFromActivity(ContextUtils.activityFromContext(context)));

        mCompositorView = new TabCompositorView(context);
        mCompositorView.onNativeLibraryLoaded(mWindowAndroid);
        mWindowAndroid.setAnimationPlaceholderView(mCompositorView);

        if (webContents != null) {
            mWebContents = webContents;
            attachWebContents(mWebContents);
        } else {
            mWebContents = WolvicWebContentsFactory.createWebContents(is_off_the_record);
        }

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

    public static TabCompositorView createNewTab(@NonNull Context context, @NonNull WebContents webContents) {
      // TODO(jfernandez): This code block is duplicated in Tabconstructor, so we may want to refactor it.
      ActivityWindowAndroid windowAndroid = new ActivityWindowAndroid(context, false,
             IntentRequestTracker.createFromActivity(ContextUtils.activityFromContext(context)));
      TabCompositorView compositorView = new TabCompositorView(context);
      compositorView.onNativeLibraryLoaded(windowAndroid);
      windowAndroid.setAnimationPlaceholderView(compositorView);
      ContentView webContentView =
             ContentView.createContentView(
                     context, /* eventOffsetHandler= */ null, webContents);
      webContents.initialize("", ViewAndroidDelegate.createBasicDelegate(webContentView),
                webContentView, windowAndroid, WebContents.createDefaultInternalsHolder());

      return compositorView;
    }

    public void destroy() {
        mWindowAndroid = null;
        mContentView = null;
        mNavigationController = null;;
        mCompositorView = null;;

        if (!mWebContents.isDestroyed()) {
            releaseWebContents(mWebContents);
        }
        mWebContents = null;
    }

    public void goBack() {
        mNavigationController.goToOffset(findBackForwardNavigationOffset(NavigationDirection.BACK));
    }

    public void goForward() {
        mNavigationController.goToOffset(findBackForwardNavigationOffset(NavigationDirection.FORWARD));
    }

    public void reload() {
        mNavigationController.reload(true);
    }

    public void loadData(@NonNull String data, @NonNull String mimeType, String encoding) {
        loadUrl(LoadUrlParams.createLoadDataParams(fixupData(data), fixupMimeType(mimeType), "base64".equals(encoding)));
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

    public void pageZoomIn() {
        ThreadUtils.runOnUiThread(() -> {
            TabJni.get().pageZoomIn(mWebContents);
        });
    }

    public void pageZoomOut() {
        ThreadUtils.runOnUiThread(() -> {
            TabJni.get().pageZoomOut(mWebContents);
        });
    }

    public void pageZoomReset() {
        ThreadUtils.runOnUiThread(() -> {
            TabJni.get().pageZoomReset(mWebContents);
        });
    }

    public int getCurrentZoomLevel() {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return TabJni.get().getCurrentZoomLevel(mWebContents);
        });
    }

    private boolean isEntryMarkedAsSkipped(int entryIndex) {
        return NAVIGATION_ENTRY_MARKED_AS_SKIPPED_VALUE.equals(
                mNavigationController.getEntryExtraData(entryIndex, NAVIGATION_ENTRY_MARKED_AS_SKIPPED_KEY));
    }

    private int findBackForwardNavigationOffset(NavigationDirection direction) {
        int currentIndex = mNavigationController.getLastCommittedEntryIndex();
        int offset = 0;
        do {
            offset += direction == NavigationDirection.BACK ? -1 : 1;
            // When the offset is out of bounds it means that we couldn't find a suitable entry
            // to go to. Return 0 to stay at the current entry.
            if (!mNavigationController.canGoToOffset(offset)) {
                return 0;
            }
        } while (isEntryMarkedAsSkipped(currentIndex + offset));

        return offset;
    }

    private static String fixupMimeType(String mimeType) {
        return TextUtils.isEmpty(mimeType) ? "text/html" : mimeType;
    }

    private static String fixupData(String data) {
        return TextUtils.isEmpty(data) ? "" : data;
    }

    private void loadUrl(LoadUrlParams params) {
	// Based on AwContents::loadUrl
        if (params.getLoadUrlType() == LoadURLType.DATA && !params.isBaseUrlDataScheme()) {
            // This allows data URLs with a non-data base URL access to file:///android_asset/ and
            // file:///android_res/ URLs.
            params.setCanLoadLocalResources(true);
        }

        // If we are reloading the same url, then set transition type as reload.
        if (params.getUrl() != null
                && params.getUrl().equals(mWebContents.getLastCommittedUrl().getSpec())
                && params.getTransitionType() == PageTransition.TYPED) {
            params.setTransitionType(PageTransition.RELOAD);
        }
        params.setTransitionType(params.getTransitionType() | PageTransition.FROM_API);
        params.setUrl(UrlFormatter.fixupUrl(params.getUrl()).getPossiblyInvalidSpec());

        mNavigationController.loadUrl(params);
    }

    @NativeMethods
    public interface Natives {
        void attachWebContents(WebContents webContents);
        void releaseWebContents(WebContents webContents);
        void setWebContentsDelegate(WebContents webContents, WolvicWebContentsDelegate delegate);
        void pageZoomIn(WebContents webContents);
        void pageZoomOut(WebContents webContents);
        void pageZoomReset(WebContents webContents);
        int getCurrentZoomLevel(WebContents webContents);
    }
}
