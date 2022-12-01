// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.components.browser_ui.modaldialog.R;
import org.chromium.components.browser_ui.modaldialog.TabModalPresenter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The presenter that displays a single tab modal dialog.
 *
 * TODO(estade): any tab modal dialog should be dismissed on system back (currently, system back
 * will navigate).
 */
public class WebLayerTabModalPresenter extends TabModalPresenter {
    private final BrowserViewController mBrowserView;
    private final Context mContext;

    /**
     * Constructor for initializing dialog container.
     * @param browserView the BrowserViewController that hosts the dialog container.
     * @param context the context used for layout inflation.
     */
    public WebLayerTabModalPresenter(BrowserViewController browserView, Context context) {
        super(context);
        mBrowserView = browserView;
        mContext = context;
    }

    @Override
    protected void showDialogContainer() {
        mBrowserView.setWebContentIsObscured(true);
        // TODO(estade): to match Chrome, don't show the dialog container before browser controls
        // are guaranteed fully visible.
        runEnterAnimation();
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        mBrowserView.setWebContentIsObscured(false);
        super.removeDialogView(model);
    }

    private FrameLayout loadDialogContainer() {
        return (FrameLayout) LayoutInflaterUtils.inflate(
                mContext, R.layout.modal_dialog_container, null);
    }

    @Override
    protected ViewGroup createDialogContainer() {
        FrameLayout dialogContainer = loadDialogContainer();
        dialogContainer.setVisibility(View.GONE);
        dialogContainer.setClickable(true);

        mBrowserView.getWebContentsOverlayView().addView(dialogContainer,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        return dialogContainer;
    }

    @Override
    protected void setBrowserControlsAccess(boolean restricted) {
        TabImpl tab = mBrowserView.getTab();
        WebContents webContents = tab.getWebContents();
        if (restricted) {
            if (webContents.isFullscreenForCurrentTab()) webContents.exitFullscreen();

            saveOrRestoreTextSelection(webContents, true);
        } else {
            saveOrRestoreTextSelection(webContents, false);
        }
    }
}
