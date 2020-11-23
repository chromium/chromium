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
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsUpdated(
            PaymentDetails details, boolean hasNotifiedInvokedPaymentApp) {
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated(String selectedShippingOptionError) {
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void complete(int result, Runnable onCompleteHandled) {
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onRetry(PaymentValidationErrors errors) {}

    // Implements BrowserPaymentRequest:
    @Override
    public void close() {
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void addPaymentAppFactories(
            PaymentAppService service, PaymentAppFactoryDelegate delegate) {
        // WebLayer only adds the GPay factory, but not using this method.
    }
}
