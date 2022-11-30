// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.payments;

import androidx.annotation.Nullable;

import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestService.Delegate;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentResponseHelper;
import org.chromium.components.payments.PaymentResponseHelperInterface;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.util.ArrayList;
import java.util.List;

/** The WebLayer-specific part of the payment request service. */
public class WebLayerPaymentRequestService implements BrowserPaymentRequest {
    private final List<PaymentApp> mAvailableApps = new ArrayList<>();
    private final JourneyLogger mJourneyLogger;
    private PaymentRequestService mPaymentRequestService;
    private PaymentRequestSpec mSpec;
    private boolean mHasClosed;
    private boolean mShouldSkipAppSelector;
    private PaymentApp mSelectedApp;

    /**
     * Create an instance of {@link WebLayerPaymentRequestService}.
     * @param paymentRequestService The payment request service.
     * @param delegate The delegate of the payment request service.
     */
    public WebLayerPaymentRequestService(
            PaymentRequestService paymentRequestService, Delegate delegate) {
        mPaymentRequestService = paymentRequestService;
        mJourneyLogger = mPaymentRequestService.getJourneyLogger();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsUpdated(
            PaymentDetails details, boolean hasNotifiedInvokedPaymentApp) {}

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated(String selectedShippingOptionError) {}

    @Override
    public boolean onPaymentAppCreated(PaymentApp paymentApp) {
        // Ignores the service worker payment apps in WebLayer until -
        // TODO(crbug.com/1224420): WebLayer supports Service worker payment apps.
        return paymentApp.getPaymentAppType() != PaymentAppType.SERVICE_WORKER_APP;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void complete(int result, Runnable onCompleteHandled) {
        onCompleteHandled.run();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onRetry(PaymentValidationErrors errors) {}

    // Implements BrowserPaymentRequest:
    @Override
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        if (mPaymentRequestService != null) {
            mPaymentRequestService.close();
            mPaymentRequestService = null;
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAvailableApps() {
        return !mAvailableApps.isEmpty();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps) {
        assert mAvailableApps.isEmpty()
            : "notifyPaymentUiOfPendingApps() should be called at most once.";
        mAvailableApps.addAll(pendingApps);
        mSelectedApp = mAvailableApps.size() == 0 ? null : mAvailableApps.get(0);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onSpecValidated(PaymentRequestSpec spec) {
        mSpec = spec;
    }

    // Implements BrowserPaymentRequest:
    @Override
    @Nullable
    public String showOrSkipAppSelector(boolean isShowWaitingForUpdatedDetails, PaymentItem total,
            boolean shouldSkipAppSelector) {
        mShouldSkipAppSelector = shouldSkipAppSelector;
        if (!mShouldSkipAppSelector) {
            return "This request is not supported in Web Layer. Please try in Chrome, or make sure "
                    + "that: (1) show() is triggered by user gesture, or"
                    + "(2) do not request any contact information.";
        }
        return null;
    }

    // Implements BrowserPaymentRequest:
    @Override
    @Nullable
    public String onShowCalledAndAppsQueriedAndDetailsFinalized() {
        assert !mAvailableApps.isEmpty()
            : "triggerPaymentAppUiSkipIfApplicable() should be called only when there is any "
                + "available app.";
        PaymentApp selectedPaymentApp = mAvailableApps.get(0);
        if (mShouldSkipAppSelector) {
            mJourneyLogger.setSkippedShow();
            PaymentResponseHelperInterface paymentResponseHelper =
                    new PaymentResponseHelper(selectedPaymentApp, mSpec.getPaymentOptions());
            mPaymentRequestService.invokePaymentApp(selectedPaymentApp, paymentResponseHelper);
        }
        return null;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public PaymentApp getSelectedPaymentApp() {
        return mAvailableApps.get(0);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public List<PaymentApp> getPaymentApps() {
        return mAvailableApps;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAnyCompleteApp() {
        return !mAvailableApps.isEmpty() && mAvailableApps.get(0).isComplete();
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage) {
        if (mPaymentRequestService != null) {
            mPaymentRequestService.disconnectFromClientWithDebugMessage(
                    debugMessage, PaymentErrorReason.USER_CANCEL);
        }
        close();
    }
}
