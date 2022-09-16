// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.payments;

import androidx.annotation.Nullable;

import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.MojoPaymentRequestGateKeeper;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentAppServiceBridge;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.PrefsStrings;
import org.chromium.components.payments.SslValidityChecker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.ProfileImpl;
import org.chromium.weblayer_private.TabImpl;

/** Creates an instance of PaymentRequest for use in WebLayer. */
public class WebLayerPaymentRequestFactory implements InterfaceFactory<PaymentRequest> {
    private final RenderFrameHost mRenderFrameHost;

    /**
     * Production implementation of the WebLayerPaymentRequestService's Delegate. Gives true answers
     * about the system.
     */
    private static class WebLayerPaymentRequestDelegateImpl
            implements PaymentRequestService.Delegate {
        private final RenderFrameHost mRenderFrameHost;

        /* package */ WebLayerPaymentRequestDelegateImpl(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
        }

        @Override
        public BrowserPaymentRequest createBrowserPaymentRequest(
                PaymentRequestService paymentRequestService) {
            return new WebLayerPaymentRequestService(paymentRequestService, this);
        }

        @Override
        public boolean isOffTheRecord() {
            ProfileImpl profile = getProfile();
            if (profile == null) return true;
            return profile.isIncognito();
        }

        @Override
        public String getInvalidSslCertificateErrorMessage() {
            WebContents webContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (webContents == null || webContents.isDestroyed()) return null;

            GURL url = webContents.getLastCommittedUrl();
            if (url == null || !OriginSecurityChecker.isSchemeCryptographic(url)) {
                return null;
            }
            return SslValidityChecker.getInvalidSslCertificateErrorMessage(webContents);
        }

        @Override
        public boolean prefsCanMakePayment() {
            BrowserContextHandle profile = getProfile();
            return profile != null
                    && UserPrefs.get(profile).getBoolean(PrefsStrings.CAN_MAKE_PAYMENT_ENABLED);
        }

        @Nullable
        @Override
        public String getTwaPackageName() {
            return null;
        }

        @Nullable
        private ProfileImpl getProfile() {
            WebContents webContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (webContents == null) return null;
            TabImpl tab = TabImpl.fromWebContents(webContents);
            if (tab == null) return null;
            return tab.getProfile();
        }
    }

    /**
     * Creates an instance of WebLayerPaymentRequestFactory.
     * @param renderFrameHost The frame that issues the payment request on the merchant page.
     */
    public WebLayerPaymentRequestFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public PaymentRequest createImpl() {
        if (mRenderFrameHost == null) return new InvalidPaymentRequest();
        if (!mRenderFrameHost.isFeatureEnabled(PermissionsPolicyFeature.PAYMENT)) {
            mRenderFrameHost.terminateRendererDueToBadMessage(241 /*PAYMENTS_WITHOUT_PERMISSION*/);
            return null;
        }

        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS)) {
            return new InvalidPaymentRequest();
        }

        PaymentRequestService.Delegate delegate =
                new WebLayerPaymentRequestDelegateImpl(mRenderFrameHost);

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null || webContents.isDestroyed()) return new InvalidPaymentRequest();
        return new MojoPaymentRequestGateKeeper(
                (client, onClosed)
                        -> new PaymentRequestService(mRenderFrameHost, client, onClosed, delegate,
                                PaymentAppServiceBridge::new));
    }
}
