// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.payments;

import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestService.Delegate;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentValidationErrors;

/** The WebLayer-specific part of the payment request service. */
public class WebLayerPaymentRequestService implements BrowserPaymentRequest {
    /**
     * Create an instance of {@link WebLayerPaymentRequestService}.
     * @param paymentRequestService The payment request service.
     * @param delegate The delegate of the payment request service.
     */
    public WebLayerPaymentRequestService(
            PaymentRequestService paymentRequestService, Delegate delegate) {
        assert false : "Not implemented yet";
    }

    @Override
    public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        assert false : "Not implemented yet";
    }

    @Override
    public void updateWith(PaymentDetails details) {
        assert false : "Not implemented yet";
    }

    @Override
    public void onPaymentDetailsNotUpdated() {
        assert false : "Not implemented yet";
    }

    @Override
    public void abort() {
        assert false : "Not implemented yet";
    }

    @Override
    public void complete(int result) {
        assert false : "Not implemented yet";
    }

    @Override
    public void retry(PaymentValidationErrors errors) {
        assert false : "Not implemented yet";
    }

    @Override
    public void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        assert false : "Not implemented yet";
    }

    @Override
    public void close() {
        assert false : "Not implemented yet";
    }

    @Override
    public void addPaymentAppFactories(PaymentAppService service) {
        assert false : "Not implemented yet";
    }

    @Override
    public PaymentAppFactoryDelegate getPaymentAppFactoryDelegate() {
        assert false : "Not implemented yet";
        return null;
    }
}
