// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic.payments;

import androidx.annotation.Nullable;

import org.chromium.components.autofill.AddressNormalizer.NormalizedAddressRequestDelegate;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentResponseHelperInterface;
import org.chromium.components.payments.PaymentAddressTypeConverter;
import org.chromium.components.payments.PaymentApp;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentResponse;

/** The helper class to create and prepare a PaymentResponse. */
public class WolvicPaymentResponseHelper
        implements NormalizedAddressRequestDelegate, PaymentResponseHelperInterface {

    private final PaymentApp mSelectedPaymentApp;
    private final PaymentOptions mPaymentOptions;
    private final PaymentResponse mPaymentResponse;
    private PaymentResponseResultCallback mResultCallback;
    private boolean mIsWaitingForShippingNormalization;
    private boolean mIsWaitingForPaymentsDetails = true;
    private PayerData mPayerDataFromPaymentApp;

    /**
     * Builds a helper to construct and fill a PaymentResponse.
     *
     * @param selectedShippingAddress The shipping address picked by the user.
     * @param selectedShippingOption The shipping option picked by the user.
     * @param selectedContact The contact info picked by the user, can be null.
     * @param selectedPaymentApp The payment app picked by the user.
     * @param paymentOptions The paymentOptions of the corresponding payment request.
     * @param personalDataManager The context appropriate PersonalDataManager reference.
     */
    public WolvicPaymentResponseHelper(
            PaymentApp selectedPaymentApp,
            PaymentOptions paymentOptions) {
        mPaymentResponse = new PaymentResponse();
        mPaymentResponse.payer = new PayerDetail();

        mSelectedPaymentApp = selectedPaymentApp;
        mPaymentOptions = paymentOptions;
    }

    @Override
    public void generatePaymentResponse(
            String methodName,
            String stringifiedDetails,
            PayerData payerData,
            PaymentResponseResultCallback resultCallback) {
        mResultCallback = resultCallback;
        mPaymentResponse.methodName = methodName;
        mPaymentResponse.stringifiedDetails = stringifiedDetails;
        mPayerDataFromPaymentApp = payerData;

        mIsWaitingForPaymentsDetails = false;

        // Wait for the shipping address normalization before sending the response.
        if (!mIsWaitingForShippingNormalization) onAllDataReady();
    }

    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        // Check if a normalization is still required.
        if (!mIsWaitingForShippingNormalization) return;
        mIsWaitingForShippingNormalization = false;

        // TODO(jfernandez): Implement the shipping address logic here.
        if (profile != null) {
            // The normalization finished first: use the normalized address.
        }

        // Wait for the payment details before sending the response.
        if (!mIsWaitingForPaymentsDetails) onAllDataReady();
    }

    @Override
    public void onCouldNotNormalize(AutofillProfile profile) {
        onAddressNormalized(profile);
    }

    private void onAllDataReady() {
        assert !mIsWaitingForPaymentsDetails;
        assert !mIsWaitingForShippingNormalization;

        // TODO(jfernandez): Set up the shipping section of the response when it comes from payment app.
        if (mPaymentOptions.requestShipping && mSelectedPaymentApp.handlesShippingAddress()) {
          mPaymentResponse.shippingAddress =
            PaymentAddressTypeConverter.convertAddressToMojoPaymentAddress(
                            mPayerDataFromPaymentApp.shippingAddress);
          mPaymentResponse.shippingOption = mPayerDataFromPaymentApp.selectedShippingOptionId;
        }

        // TODO(jfernandez): Set up the contact section of the response.
        if (mPaymentOptions.requestPayerName) {
            if (mSelectedPaymentApp.handlesPayerName()) {
                mPaymentResponse.payer.name = mPayerDataFromPaymentApp.payerName;
            } else {
              mPaymentResponse.payer.name = "";
            }
        }

        if (mPaymentOptions.requestPayerPhone) {
            if (mSelectedPaymentApp.handlesPayerPhone()) {
                mPaymentResponse.payer.phone = mPayerDataFromPaymentApp.payerPhone;
            } else {
              mPaymentResponse.payer.phone = "";
            }
        }

        if (mPaymentOptions.requestPayerEmail) {
            if (mSelectedPaymentApp.handlesPayerEmail()) {
                mPaymentResponse.payer.email = mPayerDataFromPaymentApp.payerEmail;
            } else {
                mPaymentResponse.payer.email = "";
            }
        }

        mResultCallback.onPaymentResponseReady(mPaymentResponse);
    }
}
