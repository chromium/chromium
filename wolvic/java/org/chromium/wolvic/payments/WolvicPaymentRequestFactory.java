// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic.payments;

import androidx.annotation.Nullable;

import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.MojoPaymentRequestGateKeeper;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentAppServiceBridge;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.SslValidityChecker;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceFactory;


/** Creates an instance of PaymentRequest for use in Wolvic. */
public class WolvicPaymentRequestFactory implements InterfaceFactory<PaymentRequest> {

    private final RenderFrameHost mRenderFrameHost;

    public static class WolvicPaymentRequestDelegateImpl
            implements WolvicPaymentRequestService.Delegate {
        private final RenderFrameHost mRenderFrameHost;

        private WolvicPaymentRequestDelegateImpl(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
        }

        // Implements PaymentRequestService.Delegate
        @Override
        public BrowserPaymentRequest createBrowserPaymentRequest(
                 PaymentRequestService paymentRequestService) {
          return new WolvicPaymentRequestService(paymentRequestService, this);
        }

        // Implements PaymentRequestService.Delegate
        @Override
        public boolean isOffTheRecord() {
          // TODO(jfernandez): Implement this method (it may depend on //chrome Profile.
          return false;
        }

        // Implements PaymentRequestService.Delegate
        @Override
        public String getInvalidSslCertificateErrorMessage() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) return null;
            if (!OriginSecurityChecker.isSchemeCryptographic(
                    liveWebContents.getLastCommittedUrl())) {
                return null;
            }
            return SslValidityChecker.getInvalidSslCertificateErrorMessage(liveWebContents);
        }

        // Implements PaymentRequestService.Delegate
        @Override
        public boolean prefsCanMakePayment() {
          // TODO(jfernandez): Implement this method (it may depend on //chrome Profile.
          return false;
        }


        // Implements PaymentRequestService.Delegate
        @Override
        public @Nullable String getTwaPackageName() {
          // TODO(jfernandez): Implement this method.
          return null;
        }
    }

   /**
     * Builds a factory for PaymentRequest.
     *
     * @param renderFrameHost The host of the frame that has invoked the PaymentRequest API.
     */
    public WolvicPaymentRequestFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    // Implements InterfaceFactory<PaymentRequest>
    @Override
    public PaymentRequest createImpl() {
        if (mRenderFrameHost == null) return new InvalidPaymentRequest();
        if (!mRenderFrameHost.isFeatureEnabled(PermissionsPolicyFeature.PAYMENT)) {
            mRenderFrameHost.terminateRendererDueToBadMessage(241 /*PAYMENTS_WITHOUT_PERMISSION*/);
            return null;
        }

        WolvicPaymentRequestService.Delegate delegate =
          new WolvicPaymentRequestDelegateImpl(mRenderFrameHost);

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null || webContents.isDestroyed()) return new InvalidPaymentRequest();

        return new MojoPaymentRequestGateKeeper(
                (client, onClosed) ->
                        new PaymentRequestService(
                                mRenderFrameHost,
                                client,
                                onClosed,
                                delegate,
                                PaymentAppServiceBridge::new));
    }

}
