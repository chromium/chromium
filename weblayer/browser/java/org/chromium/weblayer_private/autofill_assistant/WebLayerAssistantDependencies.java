// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.autofill_assistant;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.AssistantDependencies;
import org.chromium.components.autofill_assistant.AssistantSnackbarFactory;
import org.chromium.components.autofill_assistant.AssistantTabChangeObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implementation of {@link AssistantDependencies} for WebLayer.
 */
public class WebLayerAssistantDependencies
        extends WebLayerAssistantStaticDependencies implements AssistantDependencies {
    public WebLayerAssistantDependencies(Activity activity) {
        maybeUpdateDependencies(activity);
    }

    @Override
    public boolean maybeUpdateDependencies(Activity activity) {
        // TODO(b/222671580): Implement
        return true;
    }

    @Override
    public boolean maybeUpdateDependencies(WebContents webContents) {
        // TODO(b/222671580): Implement
        return true;
    }

    @Override
    public Activity getActivity() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public BottomSheetController getBottomSheetController() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public KeyboardVisibilityDelegate getKeyboardVisibilityDelegate() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public ApplicationViewportInsetSupplier getBottomInsetProvider() {
        // TODO(b/222671580): Implement
        return null;
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
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public Destroyable observeTabChanges(AssistantTabChangeObserver tabChangeObserver) {
        // TODO(b/222671580): Implement
        return null;
    }
}
