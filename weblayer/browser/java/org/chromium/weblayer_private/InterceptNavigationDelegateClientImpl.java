// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.os.SystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingAsyncActionType;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
public class InterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private TabImpl mTab;
    private WebContentsObserver mWebContentsObserver;
    private RedirectHandler mRedirectHandler;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private long mLastNavigationWithUserGestureTime = RedirectHandler.INVALID_TIME;
    private boolean mDestroyed;

    InterceptNavigationDelegateClientImpl(TabImpl tab) {
        mTab = tab;
        mRedirectHandler = RedirectHandler.create();
        mWebContentsObserver = new WebContentsObserver() {
            @Override
            public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
                mInterceptNavigationDelegate.onNavigationFinishedInPrimaryMainFrame(
                        navigationHandle);
            }

            @Override
            public void didFinishNavigationNoop(NavigationHandle navigation) {
                mInterceptNavigationDelegate.onNavigationFinishedNoop(navigation);
            }
        };
    }

    public void initializeWithDelegate(InterceptNavigationDelegateImpl delegate) {
        mInterceptNavigationDelegate = delegate;
        getWebContents().addObserver(mWebContentsObserver);
    }

    public void onActivityAttachmentChanged(boolean attached) {
        if (attached) {
            mInterceptNavigationDelegate.setExternalNavigationHandler(
                    createExternalNavigationHandler());
        }
    }

    public void destroy() {
        mDestroyed = true;
        getWebContents().removeObserver(mWebContentsObserver);
        mInterceptNavigationDelegate.associateWithWebContents(null);
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler() {
        return new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(mTab));
    }

    @Override
    public long getLastUserInteractionTime() {
        // NOTE: Chrome listens for user interaction with its Activity. However, this depends on
        // being able to subclass the Activity, which is not possible in WebLayer. As a proxy,
        // WebLayer uses the time of the last navigation with a user gesture to serve as the last
        // time of user interaction. Note that the user interacting with the webpage causes the
        // user gesture bit to be set on any navigation in that page for the next several seconds
        // (cf. comments on //third_party/blink/public/common/frame/user_activation_state.h). This
        // fact further increases the fidelity of this already-reasonable heuristic as a proxy. To
        // date we have not seen any concrete evidence of user-visible differences resulting from
        // the use of the different heuristic.
        return mLastNavigationWithUserGestureTime;
    }

    @Override
    public RedirectHandler getOrCreateRedirectHandler() {
        return mRedirectHandler;
    }

    @Override
    public boolean isIncognito() {
        return mTab.getProfile().isIncognito();
    }

    @Override
    public boolean isHidden() {
        return !mTab.isVisible();
    }

    @Override
    public boolean areIntentLaunchesAllowedInHiddenTabsForNavigation(
            NavigationHandle navigationHandle) {
        NavigationImpl navigation = mTab.getNavigationControllerImpl().getNavigationImplFromId(
                navigationHandle.getNavigationId());
        if (navigation == null) return false;

        return navigation.areIntentLaunchesAllowedInBackground();
    }

    @Override
    public Activity getActivity() {
        return ContextUtils.activityFromContext(mTab.getBrowser().getContext());
    }

    @Override
    public boolean wasTabLaunchedFromExternalApp() {
        return false;
    }

    @Override
    public boolean wasTabLaunchedFromLongPressInBackground() {
        return false;
    }

    @Override
    public void closeTab() {
        // When InterceptNavigationDelegate determines that a tab needs to be closed, it posts a
        // task invoking this method. It is possible that in the interim the tab was closed for
        // another reason. In that case there is nothing more to do here.
        if (mDestroyed) return;

        closeTab(mTab);
    }

    @Override
    public void onNavigationStarted(NavigationHandle navigationHandle) {
        if (navigationHandle.hasUserGesture()) {
            mLastNavigationWithUserGestureTime = SystemClock.elapsedRealtime();
        }
    }

    @Override
    public void onDecisionReachedForNavigation(
            NavigationHandle navigationHandle, OverrideUrlLoadingResult overrideUrlLoadingResult) {
        NavigationImpl navigation = mTab.getNavigationControllerImpl().getNavigationImplFromId(
                navigationHandle.getNavigationId());

        // As the navigation is still ongoing at this point there should be a NavigationImpl
        // instance for it.
        assert navigation != null;

        switch (overrideUrlLoadingResult.getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                navigation.setIntentLaunched();
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                if (overrideUrlLoadingResult.getAsyncActionType()
                        == OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH) {
                    navigation.setIsUserDecidingIntentLaunch();
                }
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB:
            case OverrideUrlLoadingResultType.NO_OVERRIDE:
            default:
                break;
        }
    }

    static void closeTab(TabImpl tab) {
        tab.getBrowser().destroyTab(tab);
    }
}
