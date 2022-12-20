// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.components.browser_ui.banners.SwipableOverlayView;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarContainerLayout;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/**
 * The {@link View} for the {@link InfoBarContainer}.
 */
public class InfoBarContainerView extends SwipableOverlayView {
    /**
     * Observes container view changes.
     */
    public interface ContainerViewObserver extends InfoBarAnimationListener {
        /**
         * Called when the height of shown content changed.
         * @param shownFraction The ratio of height of shown content to the height of the container
         *                      view.
         */
        void onShownRatioChanged(float shownFraction);
    }

    /** Top margin, including the toolbar and tabstrip height and 48dp of web contents. */
    private static final int TOP_MARGIN_PHONE_DP = 104;
    private static final int TOP_MARGIN_TABLET_DP = 144;

    /** Length of the animation to fade the InfoBarContainer back into View. */
    private static final long REATTACH_FADE_IN_MS = 250;

    /** Whether or not the InfoBarContainer is allowed to hide when the user scrolls. */
    private static boolean sIsAllowedToAutoHide = true;

    private final ContainerViewObserver mContainerViewObserver;
    private final InfoBarContainerLayout mLayout;

    /** Parent view that contains the InfoBarContainerLayout. */
    private ViewGroup mParentView;

    private TabImpl mTab;

    /** Animation used to snap the container to the nearest state if scroll direction changes. */
    private Animator mScrollDirectionChangeAnimation;

    /** Whether or not the current scroll is downward. */
    private boolean mIsScrollingDownward;

    /** Tracks the previous event's scroll offset to determine if a scroll is up or down. */
    private int mLastScrollOffsetY;

    /**
     * @param context The {@link Context} that this view is attached to.
     * @param containerViewObserver The {@link ContainerViewObserver} that gets notified on
     *                              container view changes.
     * @param isTablet Whether this view is displayed on tablet or not.
     */
    InfoBarContainerView(@NonNull Context context,
            @NonNull ContainerViewObserver containerViewObserver, TabImpl tab, boolean isTablet) {
        super(context, null);
        mTab = tab;
        mContainerViewObserver = containerViewObserver;

        // TODO(newt): move this workaround into the infobar views if/when they're scrollable.
        // Workaround for http://crbug.com/407149. See explanation in onMeasure() below.
        setVerticalScrollBarEnabled(false);

        updateLayoutParams(context, isTablet);

        Runnable makeContainerVisibleRunnable = () -> runUpEventAnimation(true);
        mLayout = new InfoBarContainerLayout(
                context, makeContainerVisibleRunnable, new InfoBarAnimationListener() {
                    @Override
                    public void notifyAnimationFinished(int animationType) {
                        mContainerViewObserver.notifyAnimationFinished(animationType);
                    }

                    @Override
                    public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
                        mContainerViewObserver.notifyAllAnimationsFinished(frontInfoBar);
                    }
                });

        addView(mLayout,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER_HORIZONTAL));
    }

    void destroy() {
        removeFromParentView();
        mTab = null;
    }

    // SwipableOverlayView implementation.
    @Override
    @VisibleForTesting
    public boolean isAllowedToAutoHide() {
        return sIsAllowedToAutoHide;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (getVisibility() != View.GONE) {
            setVisibility(VISIBLE);
            setAlpha(0f);
            animate().alpha(1f).setDuration(REATTACH_FADE_IN_MS);
        }
    }

    @Override
    protected void runUpEventAnimation(boolean visible) {
        if (mScrollDirectionChangeAnimation != null) mScrollDirectionChangeAnimation.cancel();
        super.runUpEventAnimation(visible);
    }

    @Override
    protected boolean isIndependentlyAnimating() {
        return mScrollDirectionChangeAnimation != null;
    }

    // View implementation.
    @Override
    public void setTranslationY(float translationY) {
        int contentHeightDelta = 0; // No bottom bar.

        // Push the infobar container up by any delta caused by the bottom toolbar while ensuring
        // that it does not ascend beyond the top of the bottom toolbar nor descend beyond its own
        // height.
        float newTranslationY = MathUtils.clamp(
                translationY - contentHeightDelta, -contentHeightDelta, getHeight());

        super.setTranslationY(newTranslationY);

        float shownFraction = 0;
        if (getHeight() > 0) {
            shownFraction = contentHeightDelta > 0 ? 1f : 1f - (translationY / getHeight());
        }
        mContainerViewObserver.onShownRatioChanged(shownFraction);
    }

    /**
     * Sets whether the InfoBarContainer is allowed to auto-hide when the user scrolls the page.
     * Expected to be called when Touch Exploration is enabled.
     * @param isAllowed Whether auto-hiding is allowed.
     */
    public static void setIsAllowedToAutoHide(boolean isAllowed) {
        sIsAllowedToAutoHide = isAllowed;
    }

    /**
     * Notifies that an infobar's View ({@link InfoBar#getView}) has changed. If the infobar is
     * visible, a view swapping animation will be run.
     */
    void notifyInfoBarViewChanged() {
        mLayout.notifyInfoBarViewChanged();
    }

    /**
     * Sets the parent {@link ViewGroup} that contains the {@link InfoBarContainer}.
     */
    void setParentView(ViewGroup parent) {
        mParentView = parent;
        // Don't attach the container to the new parent if it is not previously attached.
        if (removeFromParentView()) addToParentView();
    }

    /**
     * Adds this class to the parent view {@link #mParentView}.
     */
    void addToParentView() {
        // If mTab is null, destroy() was called. This should not be added after destroyed.
        assert mTab != null;
        BrowserViewController viewController =
                mTab.getBrowser().getBrowserFragment().getPossiblyNullViewController();
        if (viewController != null) {
            super.addToParentViewAtIndex(
                    mParentView, viewController.getDesiredInfoBarContainerViewIndex());
        }
    }

    /**
     * Adds an {@link InfoBar} to the layout.
     * @param infoBar The {@link InfoBar} to be added.
     */
    void addInfoBar(InfoBar infoBar) {
        infoBar.createView();
        mLayout.addInfoBar(infoBar);
    }

    /**
     * Removes an {@link InfoBar} from the layout.
     * @param infoBar The {@link InfoBar} to be removed.
     */
    void removeInfoBar(InfoBar infoBar) {
        mLayout.removeInfoBar(infoBar);
    }

    /**
     * Hides or stops hiding this View.
     * @param isHidden Whether this View is should be hidden.
     */
    void setHidden(boolean isHidden) {
        setVisibility(isHidden ? View.GONE : View.VISIBLE);
    }

    /**
     * Run an animation when the scrolling direction of a gesture has changed (this does not mean
     * the gesture has ended).
     * @param visible Whether or not the view should be visible.
     */
    private void runDirectionChangeAnimation(boolean visible) {
        mScrollDirectionChangeAnimation = createVerticalSnapAnimation(visible);
        mScrollDirectionChangeAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mScrollDirectionChangeAnimation = null;
            }
        });
        mScrollDirectionChangeAnimation.start();
    }

    @Override
    // Ensure that this view's custom layout params are passed when adding it to its parent.
    public ViewGroup.MarginLayoutParams createLayoutParams() {
        return (ViewGroup.MarginLayoutParams) getLayoutParams();
    }

    private void updateLayoutParams(Context context, boolean isTablet) {
        RelativeLayout.LayoutParams lp = new RelativeLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        lp.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        int topMarginDp = isTablet ? TOP_MARGIN_TABLET_DP : TOP_MARGIN_PHONE_DP;
        lp.topMargin = DisplayUtil.dpToPx(DisplayAndroid.getNonMultiDisplay(context), topMarginDp);
        setLayoutParams(lp);
    }

    /**
     * Returns true if any animations are pending or in progress.
     */
    @VisibleForTesting
    public boolean isAnimating() {
        return mLayout.isAnimating();
    }
}
