// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.autofill_assistant;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.AssistantDependencies;
import org.chromium.components.autofill_assistant.AssistantSnackbarFactory;
import org.chromium.components.autofill_assistant.AssistantTabChangeObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implementation of {@link AssistantDependencies} for WebLayer.
 */
public class WebLayerAssistantDependencies
        extends WebLayerAssistantStaticDependencies implements AssistantDependencies {
    public WebLayerAssistantDependencies(WebContents webContents,
            WebLayerAssistantTabChangeObserver webLayerAssistantTabChangeObserver) {
        super(webContents, webLayerAssistantTabChangeObserver);
        maybeUpdateDependencies(webContents);
    }

    /**
     * Returns true if all dependencies are available.
     */
    @Override
    public boolean maybeUpdateDependencies(WebContents webContents) {
        assert webContents == mWebContents;
        return getWindowAndroid() != null && getActivity() != null;
    }

    @Override
    @Nullable
    public WindowAndroid getWindowAndroid() {
        return mWebContents.getTopLevelNativeWindow();
    }

    @Override
    @Nullable
    public Activity getActivity() {
        WindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null) return null;

        return windowAndroid.getActivity().get();
    }

    @Override
    @Nullable
    public BottomSheetController getBottomSheetController() {
        WindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null) return null;

        return BottomSheetControllerProvider.from(windowAndroid);
    }

    @Override
    @Nullable
    public KeyboardVisibilityDelegate getKeyboardVisibilityDelegate() {
        WindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null) return null;

        return windowAndroid.getKeyboardDelegate();
    }

    @Override
    @Nullable
    public ApplicationViewportInsetSupplier getBottomInsetProvider() {
        WindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null) return null;

        return windowAndroid.getApplicationBottomInsetProvider();
    }

    @Override
    public View getRootView() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public ViewGroup getRootViewGroup() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public AssistantSnackbarFactory getSnackbarFactory() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public AssistantBrowserControlsFactory createBrowserControlsFactory() {
        return () -> new WebLayerAssistantBrowserControls();
    }

    @Override
    public Destroyable observeTabChanges(AssistantTabChangeObserver tabChangeObserver) {
        mWebLayerAssistantTabChangeObserver.addObserver(tabChangeObserver);
        return () -> mWebLayerAssistantTabChangeObserver.removeObserver(tabChangeObserver);
    }
}
