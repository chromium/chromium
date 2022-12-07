// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.RemoteException;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.ExternalIntentInIncognitoUserDecision;

import java.util.List;

/**
 * WebLayer's implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    private final TabImpl mTab;
    private boolean mTabDestroyed;

    public ExternalNavigationDelegateImpl(TabImpl tab) {
        assert tab != null;
        mTab = tab;
    }

    public void onTabDestroyed() {
        mTabDestroyed = true;
    }

    @Override
    public Context getContext() {
        return mTab.getBrowser().getContext();
    }

    @Override
    public boolean willAppHandleIntent(Intent intent) {
        return false;
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(GURL url) {
        return false;
    }

    @Override
    public boolean shouldAvoidDisambiguationDialog(Intent intent) {
        // Don't show the disambiguation dialog if WebLayer can handle the intent.
        return UrlUtilities.isAcceptedScheme(intent.toUri(0));
    }

    @Override
    public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {
        if (!hasValidTab()) return;
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public boolean isApplicationInForeground() {
        return mTab.getBrowser().isResumed();
    }

    @Override
    public void maybeSetWindowId(Intent intent) {}

    @Override
    public boolean canLoadUrlInCurrentTab() {
        return true;
    }

    @Override
    public void closeTab() {
        InterceptNavigationDelegateClientImpl.closeTab(mTab);
    }

    @Override
    public boolean isIncognito() {
        return mTab.getProfile().isIncognito();
    }

    @Override
    public boolean hasCustomLeavingIncognitoDialog() {
        return mTab.getExternalIntentInIncognitoCallbackProxy() != null;
    }

    @Override
    public void presentLeavingIncognitoModalDialog(Callback<Boolean> onUserDecision) {
        try {
            mTab.getExternalIntentInIncognitoCallbackProxy().onExternalIntentInIncognito(
                    (Integer result) -> {
                        @ExternalIntentInIncognitoUserDecision
                        int userDecision = result.intValue();
                        switch (userDecision) {
                            case ExternalIntentInIncognitoUserDecision.ALLOW:
                                onUserDecision.onResult(Boolean.valueOf(true));
                                break;
                            case ExternalIntentInIncognitoUserDecision.DENY:
                                onUserDecision.onResult(Boolean.valueOf(false));
                                break;
                            default:
                                assert false;
                        }
                    });
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetRequestMetadata(
            Intent intent, boolean hasUserGesture, boolean isRendererInitiated) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingReferrer(Intent intent, GURL referrerUrl) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingIncognitoUrl(Intent intent) {}

    @Override
    public WindowAndroid getWindowAndroid() {
        return mTab.getBrowser().getWindowAndroid();
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public boolean hasValidTab() {
        assert mTab != null;
        return !mTabDestroyed;
    }

    @Override
    public boolean canCloseTabOnIncognitoIntentLaunch() {
        return hasValidTab();
    }

    @Override
    public boolean isIntentForTrustedCallingApp(
            Intent intent, Supplier<List<ResolveInfo>> resolveInfoSupplier) {
        return false;
    }

    @Override
    public boolean shouldLaunchWebApksOnInitialIntent() {
        return false;
    }

    @Override
    public boolean maybeSetTargetPackage(
            Intent intent, Supplier<List<ResolveInfo>> resolveInfoSupplier) {
        return false;
    }

    @Override
    public boolean shouldEmbedderInitiatedNavigationsStayInBrowser() {
        // WebLayer already has APIs that allow the embedder to specify that a navigation shouldn't
        // result in an external intent (Navigation#disableIntentProcessing() and
        // NavigateParams#disableIntentProcessing()), and historically embedder-initiated
        // navigations have been allowed to leave the browser on the initial navigation, so we need
        // to maintain that behavior.
        return false;
    }
}
