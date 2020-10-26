// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.payments;

import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceFactory;

/** Creates an instance of PaymentRequest for use in WebLayer. */
public class WebLayerPaymentRequestFactory implements InterfaceFactory<PaymentRequest> {
    private final RenderFrameHost mRenderFrameHost;

    /**
     * Creates an instance of WebLayerPaymentRequestFactory.
     * @param renderFrameHost The frame that issues the payment request on the merchant page.
     */
    public WebLayerPaymentRequestFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public PaymentRequest createImpl() {
        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS)) {
            return new InvalidPaymentRequest();
        }

        // TODO(maxlg): replace the returned value with a PaymentRequest.
        return null;
    }
}
