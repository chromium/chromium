// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.Resources;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.webkit.ValueCallback;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * BrowserViewController controls the set of Views needed to show the WebContents.
 */
@JNINamespace("weblayer")
public final class BrowserViewController
        implements BrowserControlsContainerView.Listener,
                   WebContentsGestureStateTracker.OnGestureStateChangedListener,
                   ModalDialogManager.ModalDialogManagerObserver {
    private final ContentViewRenderView mContentViewRenderView;
    // Child of mContentViewRenderView. Be very careful adding Views to this, as any Views are not
    // accessible (ContentView provides it's own accessible implementation that interacts with
    // WebContents).
    private final ContentView mContentView;
    // Child of mContentViewRenderView, holds top-view from client.
    private final BrowserControlsContainerView mTopControlsContainerView;
    // Child of mContentViewRenderView, holds bottom-view from client.
    private final BrowserControlsContainerView mBottomControlsContainerView;
    // Other child of mContentViewRenderView, which holds views that sit on top of the web contents,
    // such as tab modal dialogs.
    private final FrameLayout mWebContentsOverlayView;
    // Child of mContentViewRenderView. This view has a top margin matching the current state of the
    // top controls, which allows the autofill popup to be positioned correctly.
    private final AutofillView mAutofillView;
    private final RelativeLayout.LayoutParams mAutofillParams = new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT);

    private final FragmentWindowAndroid mWindowAndroid;
    private final ModalDialogManager mModalDialogManager;

    private TabImpl mTab;

    private WebContentsGestureStateTracker mGestureStateTracker;

    /**
     * The value of mCachedDoBrowserControlsShrinkRendererSize is set when
     * WebContentsGestureStateTracker begins a gesture. This is necessary as the values should only
     * change once a gesture is no longer under way.
     */
    private boolean mCachedDoBrowserControlsShrinkRendererSize;

    public BrowserViewController(FragmentWindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        Context context = mWindowAndroid.getContext().get();
        mContentViewRenderView = new ContentViewRenderView(context);

        mContentViewRenderView.onNativeLibraryLoaded(
                mWindowAndroid, ContentViewRenderView.MODE_SURFACE_VIEW);
        mTopControlsContainerView =
                new BrowserControlsContainerView(context, mContentViewRenderView, this, true);
        mTopControlsContainerView.setId(View.generateViewId());
        mBottomControlsContainerView =
                new BrowserControlsContainerView(context, mContentViewRenderView, this, false);
        mBottomControlsContainerView.setId(View.generateViewId());
        mContentView = ContentView.createContentView(
                context, mTopControlsContainerView.getEventOffsetHandler(), null /* webContents */);
        mContentViewRenderView.addView(mContentView,
                new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT,
                        RelativeLayout.LayoutParams.MATCH_PARENT));
        mContentViewRenderView.addView(mTopControlsContainerView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        RelativeLayout.LayoutParams bottomControlsContainerViewParams =
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        bottomControlsContainerViewParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        mContentViewRenderView.addView(
                mBottomControlsContainerView, bottomControlsContainerViewParams);

        mWebContentsOverlayView = new FrameLayout(context);
        RelativeLayout.LayoutParams overlayParams =
                new RelativeLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0);
        overlayParams.addRule(RelativeLayout.BELOW, mTopControlsContainerView.getId());
        overlayParams.addRule(RelativeLayout.ABOVE, mBottomControlsContainerView.getId());
        mContentViewRenderView.addView(mWebContentsOverlayView, overlayParams);
        mWindowAndroid.setAnimationPlaceholderView(mWebContentsOverlayView);

        mModalDialogManager = new ModalDialogManager(
                new AppModalPresenter(context), ModalDialogManager.ModalDialogType.APP);
        mModalDialogManager.addObserver(this);
        mModalDialogManager.registerPresenter(
                new WebLayerTabModalPresenter(this, context), ModalDialogType.TAB);
        mWindowAndroid.setModalDialogManager(mModalDialogManager);

        mAutofillView = new AutofillView(context);
        mContentViewRenderView.addView(mAutofillView, mAutofillParams);
    }

    public void destroy() {
        mWindowAndroid.setModalDialogManager(null);
        setActiveTab(null);
        mTopControlsContainerView.destroy();
        mBottomControlsContainerView.destroy();
        mContentViewRenderView.destroy();
    }

    /** Returns top-level View this Controller works with */
    public View getView() {
        return mContentViewRenderView;
    }

    /** Returns the ViewGroup into which the InfoBarContainer should be parented. **/
    public ViewGroup getInfoBarContainerParentView() {
        return mContentViewRenderView;
    }

    public ViewGroup getContentView() {
        return mContentView;
    }

    public FrameLayout getWebContentsOverlayView() {
        return mWebContentsOverlayView;
    }

    public ViewGroup getAutofillView() {
        return mAutofillView;
    }

    // Returns the index at which the infobar container view should be inserted.
    public int getDesiredInfoBarContainerViewIndex() {
        // Ensure that infobars are positioned behind WebContents overlays in z-order.
        // TODO(blundell): Should infobars instead be hidden while a WebContents overlay is
        // presented?
        return mContentViewRenderView.indexOfChild(mWebContentsOverlayView) - 1;
    }

    public void setActiveTab(TabImpl tab) {
        if (tab == mTab) return;

        if (mTab != null) {
            mTab.onDidLoseActive();
            // WebContentsGestureStateTracker is relatively cheap, easier to destroy rather than
            // update WebContents.
            mGestureStateTracker.destroy();
            mGestureStateTracker = null;
        }

        mModalDialogManager.dismissDialogsOfType(
                ModalDialogType.TAB, DialogDismissalCause.TAB_SWITCHED);

        mTab = tab;
        WebContents webContents = mTab != null ? mTab.getWebContents() : null;
        // Create the WebContentsGestureStateTracker before setting the WebContents on
        // the views as they may call back to this class.
        if (mTab != null) {
            mGestureStateTracker =
                    new WebContentsGestureStateTracker(mContentView, webContents, this);
        }
        mAutofillView.setTab(mTab);

        mContentView.setWebContents(webContents);
        mContentViewRenderView.setWebContents(webContents);
        mTopControlsContainerView.setWebContents(webContents);
        mBottomControlsContainerView.setWebContents(webContents);
        if (mTab != null) {
            mTab.onDidGainActive(mTopControlsContainerView.getNativeHandle(),
                    mBottomControlsContainerView.getNativeHandle());
            mContentView.requestFocus();
        }
    }

    public TabImpl getTab() {
        return mTab;
    }

    public void setTopView(View view) {
        mTopControlsContainerView.setView(view);
    }

    public void setBottomView(View view) {
        mBottomControlsContainerView.setView(view);
    }

    public int getBottomContentHeightDelta() {
        return mBottomControlsContainerView.getContentHeightDelta();
    }

    public boolean compositorHasSurface() {
        return mContentViewRenderView.hasSurface();
    }

    public void setWebContentIsObscured(boolean isObscured) {
        mContentView.setIsObscuredForAccessibility(isObscured);
    }

    @Override
    public void onBrowserControlsCompletelyShownOrHidden() {
        adjustWebContentsHeightIfNecessary();
    }

    @Override
    public void onGestureStateChanged() {
        // This is called from |mGestureStateTracker|.
        assert mGestureStateTracker != null;
        if (mGestureStateTracker.isInGestureOrScroll()) {
            mCachedDoBrowserControlsShrinkRendererSize =
                    mTopControlsContainerView.isControlVisible()
                    || mBottomControlsContainerView.isControlVisible();
        }
        adjustWebContentsHeightIfNecessary();
    }

    @Override
    public void onDialogAdded(PropertyModel model) {
        onDialogVisibilityChanged(true);
    }

    @Override
    public void onLastDialogDismissed() {
        onDialogVisibilityChanged(false);
    }

    private void onDialogVisibilityChanged(boolean showing) {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 82) return;

        // ModalDialogManager.onLastDialogDismissed() may be called if |mTab| is currently null.
        // This is because in some situations ModalDialogManager calls onLastDialogDismissed() even
        // if there were no dialogs present and dismissDialog() is called. This matters as
        // dismissDialog() may be called when |mTab| is null.
        // TODO(sky): fix ModalDialogManager and remove mTab conditional.
        if (mModalDialogManager.getCurrentType() == ModalDialogType.TAB && mTab != null) {
            try {
                mTab.getClient().onTabModalStateChanged(showing);
            } catch (RemoteException e) {
                throw new AndroidRuntimeException(e);
            }
        }
    }

    private void adjustWebContentsHeightIfNecessary() {
        if (mGestureStateTracker == null || mGestureStateTracker.isInGestureOrScroll()
                || !mTopControlsContainerView.isCompletelyShownOrHidden()
                || !mBottomControlsContainerView.isCompletelyShownOrHidden()) {
            return;
        }
        mContentViewRenderView.setWebContentsHeightDelta(
                mTopControlsContainerView.getContentHeightDelta()
                + mBottomControlsContainerView.getContentHeightDelta());

        mAutofillParams.topMargin = mTopControlsContainerView.getContentHeightDelta();
        mAutofillView.setLayoutParams(mAutofillParams);
    }

    public void setSupportsEmbedding(boolean enable, ValueCallback<Boolean> callback) {
        mContentViewRenderView.requestMode(enable ? ContentViewRenderView.MODE_TEXTURE_VIEW
                                                  : ContentViewRenderView.MODE_SURFACE_VIEW,
                callback);
    }

    public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
        mTopControlsContainerView.onOffsetsChanged(topControlsOffsetY, topContentOffsetY);
    }

    public void onBottomControlsChanged(int bottomControlsOffsetY) {
        mBottomControlsContainerView.onOffsetsChanged(bottomControlsOffsetY, 0);
    }

    public boolean doBrowserControlsShrinkRendererSize() {
        return mGestureStateTracker.isInGestureOrScroll()
                ? mCachedDoBrowserControlsShrinkRendererSize
                : (mTopControlsContainerView.isControlVisible()
                        || mBottomControlsContainerView.isControlVisible());
    }

    /**
     * Causes the browser controls to be fully shown. Take care in calling this. Normally the
     * renderer drives the offsets, but this method circumvents that.
     */
    public void showControls() {
        mTopControlsContainerView.onOffsetsChanged(0, mTopControlsContainerView.getHeight());
        mBottomControlsContainerView.onOffsetsChanged(0, 0);
    }

    /**
     * @return true if a tab modal was showing and has been dismissed.
     */
    public boolean dismissTabModalOverlay() {
        return mModalDialogManager.dismissActiveDialogOfType(
                ModalDialogType.TAB, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }

    /**
     * Asks the user to confirm a page reload on a POSTed page.
     */
    public void showRepostFormWarningDialog() {
        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(mModalDialogManager, (Integer dismissalCause) -> {
                    WebContents webContents = mTab == null ? null : mTab.getWebContents();
                    if (webContents == null) return;
                    switch (dismissalCause) {
                        case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                            webContents.getNavigationController().continuePendingReload();
                            break;
                        default:
                            webContents.getNavigationController().cancelPendingReload();
                            break;
                    }
                });

        Resources resources = mWindowAndroid.getContext().get().getResources();
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, resources,
                                R.string.http_post_warning_title)
                        .with(ModalDialogProperties.MESSAGE, resources, R.string.http_post_warning)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.http_post_warning_resend)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mModalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.TAB, true);
    }
}
