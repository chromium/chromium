// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
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

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.SystemUiScrimDelegate;
import org.chromium.components.content_capture.ContentCaptureConsumer;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetController;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;
import org.chromium.weblayer_private.interfaces.BrowserEmbeddabilityMode;

/**
 * BrowserViewController controls the set of Views needed to show the WebContents.
 */
@JNINamespace("weblayer")
public final class BrowserViewController
        implements BrowserControlsContainerView.Delegate,
                   WebContentsGestureStateTracker.OnGestureStateChangedListener,
                   ModalDialogManager.ModalDialogManagerObserver {
    /** Information needed to restore the UI state after recreating the BrowserViewController. */
    /* package */ static class State {
        private BrowserControlsContainerView.State mTopControlsState;
        private BrowserControlsContainerView.State mBottomControlsState;

        private State(BrowserControlsContainerView.State topControlsState,
                BrowserControlsContainerView.State bottomControlsState) {
            mTopControlsState = topControlsState;
            mBottomControlsState = bottomControlsState;
        }
    }

    private final ContentViewRenderView mContentViewRenderView;
    // Child of mContentViewRenderView. Be very careful adding Views to this, as any Views are not
    // accessible (ContentView provides it's own accessible implementation that interacts with
    // WebContents).
    private final ContentView mContentView;
    // Child of mContentViewRenderView, holds the SurfaceView for WebXR.
    private final FrameLayout mArViewHolder;
    // Child of mContentViewRenderView, holds top-view from client.
    private final BrowserControlsContainerView mTopControlsContainerView;
    // Child of mContentViewRenderView, holds bottom-view from client.
    private final BrowserControlsContainerView mBottomControlsContainerView;
    // Other child of mContentViewRenderView, which holds views that sit on top of the web contents,
    // such as tab modal dialogs.
    private final FrameLayout mWebContentsOverlayView;

    private final FragmentWindowAndroid mWindowAndroid;
    private final View.OnAttachStateChangeListener mOnAttachedStateChangeListener;
    private final ModalDialogManager mModalDialogManager;

    private final ScrimCoordinator mScrim;

    private final ViewGroup mBottomSheetContainer;
    private final ManagedBottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;

    private PwaBottomSheetController mPwaBottomSheetController;

    private TabImpl mTab;

    private WebContentsGestureStateTracker mGestureStateTracker;

    @BrowserControlsState
    private int mBrowserControlsConstraint = BrowserControlsState.BOTH;

    /**
     * The value of mCachedDoBrowserControlsShrinkRendererSize is set when
     * WebContentsGestureStateTracker begins a gesture. This is necessary as the values should only
     * change once a gesture is no longer under way.
     */
    private boolean mCachedDoBrowserControlsShrinkRendererSize;

    private OnscreenContentProvider mOnscreenContentProvider;

    public BrowserViewController(FragmentWindowAndroid windowAndroid,
            View.OnAttachStateChangeListener listener, @Nullable State savedState,
            boolean recreateForConfigurationChange) {
        mWindowAndroid = windowAndroid;
        mOnAttachedStateChangeListener = listener;
        Context context = mWindowAndroid.getContext().get();
        mContentViewRenderView = new ContentViewRenderView(context, recreateForConfigurationChange);
        mContentViewRenderView.addOnAttachStateChangeListener(listener);

        mContentViewRenderView.onNativeLibraryLoaded(
                mWindowAndroid, BrowserEmbeddabilityMode.UNSUPPORTED);
        mTopControlsContainerView =
                new BrowserControlsContainerView(context, mContentViewRenderView, this, true,
                        (savedState == null) ? null : savedState.mTopControlsState);
        mTopControlsContainerView.setId(View.generateViewId());
        mBottomControlsContainerView =
                new BrowserControlsContainerView(context, mContentViewRenderView, this, false,
                        (savedState == null) ? null : savedState.mBottomControlsState);
        mBottomControlsContainerView.setId(View.generateViewId());
        mContentView = ContentViewWithAutofill.createContentView(
                context, mTopControlsContainerView.getEventOffsetHandler());
        mContentViewRenderView.addView(mContentView,
                new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT,
                        RelativeLayout.LayoutParams.MATCH_PARENT));

        mArViewHolder = new FrameLayout(context);
        mArViewHolder.setVisibility(View.GONE);
        mContentViewRenderView.addView(mArViewHolder,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

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

        SystemUiScrimDelegate systemUiDelegate = new SystemUiScrimDelegate() {
            @Override
            public void setStatusBarScrimFraction(float scrimFraction) {
                // TODO(mdjones): Support status bar tinting if it is needed by WebLayer.
            }

            @Override
            public void setNavigationBarScrimFraction(float scrimFraction) {}
        };
        mScrim = new ScrimCoordinator(context, systemUiDelegate, mContentViewRenderView,
                context.getResources().getColor(R.color.default_scrim_color));
        mBottomSheetContainer = new FrameLayout(context);
        mBottomSheetContainer.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mBottomSheetContainer.setClipChildren(false);
        mBottomSheetContainer.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View view, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Allow the sheet container to layout once before hiding it until it is used.
                mBottomSheetContainer.setVisibility(View.GONE);
                mBottomSheetContainer.removeOnLayoutChangeListener(this);
            }
        });
        mContentViewRenderView.addView(mBottomSheetContainer);

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) {
            // TODO(rayankans): Remove assumptions about Activity from BottomSheetController.
            mBottomSheetController = null;
            mPwaBottomSheetController = null;
            mBottomSheetObserver = null;
            return;
        }
        mBottomSheetController = BottomSheetControllerFactory.createBottomSheetController(
                () -> mScrim, (v) -> {}, activity.getWindow(),
                KeyboardVisibilityDelegate.getInstance(), () -> mBottomSheetContainer,
                () -> mContentViewRenderView.getHeight());
        BottomSheetControllerFactory.attach(mWindowAndroid, mBottomSheetController);

        mPwaBottomSheetController = PwaBottomSheetControllerFactory.createPwaBottomSheetController(
                mWindowAndroid.getContext().get());
        PwaBottomSheetControllerFactory.attach(mWindowAndroid, mPwaBottomSheetController);
        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            /** A token for suppressing app modal dialogs. */
            private int mAppModalToken = TokenHolder.INVALID_TOKEN;

            /** A token for suppressing tab modal dialogs. */
            private int mTabModalToken = TokenHolder.INVALID_TOKEN;

            @Override
            public void onSheetOpened(int reason) {
                assert mAppModalToken == TokenHolder.INVALID_TOKEN;
                assert mTabModalToken == TokenHolder.INVALID_TOKEN;
                mAppModalToken =
                        mModalDialogManager.suspendType(ModalDialogManager.ModalDialogType.APP);
                mTabModalToken =
                        mModalDialogManager.suspendType(ModalDialogManager.ModalDialogType.TAB);
            }

            @Override
            public void onSheetClosed(int reason) {
                if (mAppModalToken != TokenHolder.INVALID_TOKEN
                        || mTabModalToken != TokenHolder.INVALID_TOKEN) {
                    // If one modal dialog token is set, the other should be as well.
                    assert mAppModalToken != TokenHolder.INVALID_TOKEN
                            && mTabModalToken != TokenHolder.INVALID_TOKEN;
                    mModalDialogManager.resumeType(
                            ModalDialogManager.ModalDialogType.APP, mAppModalToken);
                    mModalDialogManager.resumeType(
                            ModalDialogManager.ModalDialogType.TAB, mTabModalToken);
                }
                mAppModalToken = TokenHolder.INVALID_TOKEN;
                mTabModalToken = TokenHolder.INVALID_TOKEN;
            }
        };
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.setAccessibilityUtil(WebLayerAccessibilityUtil.get());
    }

    public void destroy() {
        if (mBottomSheetController != null) {
            BottomSheetControllerFactory.detach(mBottomSheetController);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
            PwaBottomSheetControllerFactory.detach(mPwaBottomSheetController);
        }
        mWindowAndroid.setModalDialogManager(null);
        setActiveTab(null);
        if (mOnscreenContentProvider != null) mOnscreenContentProvider.destroy();
        mContentViewRenderView.removeOnAttachStateChangeListener(mOnAttachedStateChangeListener);
        mTopControlsContainerView.destroy();
        mBottomControlsContainerView.destroy();
        mContentViewRenderView.destroy();
    }

    /** Returns top-level View this Controller works with */
    public View getView() {
        return mContentViewRenderView;
    }

    public InsetObserverView getInsetObserverView() {
        return mContentViewRenderView.getInsetObserverView();
    }

    /** Returns the ViewGroup into which the InfoBarContainer should be parented. **/
    public ViewGroup getInfoBarContainerParentView() {
        return mContentViewRenderView;
    }

    public ContentView getContentView() {
        return mContentView;
    }

    public FrameLayout getWebContentsOverlayView() {
        return mWebContentsOverlayView;
    }

    public ViewGroup getArViewHolder() {
        return mArViewHolder;
    }

    public void setSurfaceProperties(boolean requiresAlphaChannel, boolean zOrderMediaOverlay) {
        mContentViewRenderView.setSurfaceProperties(requiresAlphaChannel, zOrderMediaOverlay);
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
            mTab.onDetachedFromViewController();
            mTab.setBrowserControlsVisibilityConstraint(
                    ImplControlsVisibilityReason.ANIMATION, BrowserControlsState.BOTH);
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
        mContentView.setWebContents(webContents);

        mContentViewRenderView.setWebContents(webContents);
        mTopControlsContainerView.setWebContents(webContents);
        mBottomControlsContainerView.setWebContents(webContents);
        if (mTab != null) {
            mTab.setBrowserControlsVisibilityConstraint(
                    ImplControlsVisibilityReason.ANIMATION, mBrowserControlsConstraint);
            mTab.setOnlyExpandTopControlsAtPageTop(
                    mTopControlsContainerView.getOnlyExpandControlsAtPageTop());
            mTab.onAttachedToViewController(mTopControlsContainerView.getNativeHandle(),
                    mBottomControlsContainerView.getNativeHandle());
            mContentView.requestFocus();
        }

        if (mOnscreenContentProvider == null) {
            mOnscreenContentProvider = new OnscreenContentProvider(
                    mWindowAndroid.getContext().get(), mContentViewRenderView, webContents);
        } else {
            mOnscreenContentProvider.onWebContentsChanged(webContents);
        }
    }

    public TabImpl getTab() {
        return mTab;
    }

    public void setTopView(View view) {
        mTopControlsContainerView.setView(view);
    }

    public void setTopControlsMinHeight(int minHeight) {
        mTopControlsContainerView.setMinHeight(minHeight);
    }

    public void setOnlyExpandTopControlsAtPageTop(boolean onlyExpandControlsAtPageTop) {
        if (onlyExpandControlsAtPageTop
                == mTopControlsContainerView.getOnlyExpandControlsAtPageTop()) {
            return;
        }
        mTopControlsContainerView.setOnlyExpandControlsAtPageTop(onlyExpandControlsAtPageTop);
        if (mTab == null) return;
        mTab.setOnlyExpandTopControlsAtPageTop(onlyExpandControlsAtPageTop);
    }

    public void addContentCaptureConsumerForTesting(ContentCaptureConsumer consumer) {
        mOnscreenContentProvider.addConsumer(consumer);
    }

    public void setTopControlsAnimationsEnabled(boolean animationsEnabled) {
        mTopControlsContainerView.setAnimationsEnabled(animationsEnabled);
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

    public View getViewForMagnifierReadback() {
        return mContentViewRenderView.getViewForMagnifierReadback();
    }

    @Override
    public void refreshPageHeight() {
        adjustWebContentsHeightIfNecessary();
    }

    @Override
    public void setAnimationConstraint(@BrowserControlsState int constraint) {
        mBrowserControlsConstraint = constraint;
        if (mTab == null) return;
        mTab.setBrowserControlsVisibilityConstraint(
                ImplControlsVisibilityReason.ANIMATION, constraint);
    }

    @Override
    public void onOffsetsChanged(boolean isTop, int controlsOffset) {
        if (mTab == null) return;
        mTab.getBrowser().onBrowserControlsOffsetsChanged(mTab, isTop, controlsOffset);
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

    /* package */ State getState() {
        return new State(
                mTopControlsContainerView.getState(), mBottomControlsContainerView.getState());
    }

    private void onDialogVisibilityChanged(boolean showing) {
        if (mModalDialogManager.getCurrentType() == ModalDialogType.TAB) {
            // This shouldn't be called when |mTab| is null and the modal dialog type is TAB. OTOH,
            // when an app-modal is displayed for a javascript dialog, this method can be called
            // after the tab is destroyed.
            assert mTab != null;
            try {
                mTab.getClient().onTabModalStateChanged(showing);
            } catch (RemoteException e) {
                throw new AndroidRuntimeException(e);
            }
        }
    }

    private void adjustWebContentsHeightIfNecessary() {
        if (mGestureStateTracker == null || mGestureStateTracker.isInGestureOrScroll()
                || !mTopControlsContainerView.isCompletelyExpandedOrCollapsed()
                || !mBottomControlsContainerView.isCompletelyExpandedOrCollapsed()) {
            return;
        }
        mContentViewRenderView.setWebContentsHeightDelta(
                mTopControlsContainerView.getContentHeightDelta()
                + mBottomControlsContainerView.getContentHeightDelta());
    }

    public void setEmbeddabilityMode(
            @BrowserEmbeddabilityMode int mode, ValueCallback<Boolean> callback) {
        mContentViewRenderView.requestMode(mode, callback);
    }

    public void setMinimumSurfaceSize(int width, int height) {
        mContentViewRenderView.setMinimumSurfaceSize(width, height);
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

    public boolean shouldAnimateBrowserControlsHeightChanges() {
        return mTopControlsContainerView.shouldAnimateBrowserControlsHeightChanges();
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
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(R.string.http_post_warning))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.http_post_warning_resend)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mModalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.TAB, true);
    }
}
