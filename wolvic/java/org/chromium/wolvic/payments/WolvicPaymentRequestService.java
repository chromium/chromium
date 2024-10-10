// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic.payments;

import androidx.annotation.Nullable;
import android.content.Context;

import org.chromium.base.Log;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentRequestUpdateEventListener;
import org.chromium.components.payments.PaymentResponseHelperInterface;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

import org.chromium.wolvic.payments.ui.WolvicPaymentUiService;

import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Map;

/**
 * This is the Clank specific parts of {@link PaymentRequest}
 */
public class WolvicPaymentRequestService
        implements BrowserPaymentRequest, WolvicPaymentUiService.Delegate {

  // Null checks is needed because there may be owners of WolvicPaymentRequestService instances after
  // mPaymentRequestService is set to null (issues/40715891)
  @Nullable private PaymentRequestService mPaymentRequestService;

  private final RenderFrameHost mRenderFrameHost;
  private final Delegate mDelegate;
  private final WebContents mWebContents;
  private final JourneyLogger mJourneyLogger;

  private final WolvicPaymentUiService mPaymentUiService;
  private boolean mWasRetryCalled;

  private boolean mHasClosed;

  private PaymentRequestSpec mSpec;
  private PaymentHandlerHost mPaymentHandlerHost;

  private static final String TAG = "WolvicPaymentRequestService";

  /**
   * True if the browser has skipped showing the app selector UI (PaymentRequest UI).
   *
   * <p>In cases where there is a single payment app and the merchant does not request shipping
   * or billing, the browser can skip showing UI as the app selector UI is not benefiting the user
   * at all.
   */
  private boolean mHasSkippedAppSelector;

  /** The delegate of this class */
  public interface Delegate extends PaymentRequestService.Delegate {
        /**
         * Create WolvicPaymentUiService.
         * @param delegate The delegate of this instance.
         * @param webContents The WebContents of the merchant page.
         * @param isOffTheRecord Whether merchant page is in an isOffTheRecord tab.
         * @param journeyLogger The logger of the user journey.
         * @param topLevelOrigin The last committed url of webContents.
         */
        default WolvicPaymentUiService createPaymentUiService(
                WolvicPaymentUiService.Delegate delegate,
                PaymentRequestParams params,
                WebContents webContents,
                boolean isOffTheRecord,
                JourneyLogger journeyLogger,
                String topLevelOrigin) {
            return new WolvicPaymentUiService(
                    /* delegate= */ delegate,
                    /* params= */ params,
                    webContents,
                    isOffTheRecord,
                    journeyLogger,
                    topLevelOrigin);
        }

       /**
         * Creates an instance of PaymentHandlerHost.
         * @param webContents The WebContents that issues the payment request.
         * @param listener The listener to payment method, shipping address, and shipping option
         *        change events
         * @return The instance.
         */
        default PaymentHandlerHost createPaymentHandlerHost(
                WebContents webContents, PaymentRequestUpdateEventListener listener) {
            return new PaymentHandlerHost(webContents, listener);
        }
  }

    /**
     * Builds the PaymentRequest service implementation.
     *
     * @param paymentRequestService The component side of the PaymentRequest implementation.
     * @param delegate The delegate of this class.
     */
    public WolvicPaymentRequestService(
            PaymentRequestService paymentRequestService, Delegate delegate) {
        assert paymentRequestService != null;
        assert delegate != null;

        mPaymentRequestService = paymentRequestService;
        mRenderFrameHost = paymentRequestService.getRenderFrameHost();
        assert mRenderFrameHost != null;
        mDelegate = delegate;
        mWebContents = paymentRequestService.getWebContents();
        mJourneyLogger = paymentRequestService.getJourneyLogger();
        String topLevelOrigin = paymentRequestService.getTopLevelOrigin();
        assert topLevelOrigin != null;
        mPaymentUiService =
                mDelegate.createPaymentUiService(
                        /* delegate= */ this,
                        /* params= */ paymentRequestService,
                        mWebContents,
                        paymentRequestService.isOffTheRecord(),
                        mJourneyLogger,
                        topLevelOrigin);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public PaymentApp getSelectedPaymentApp() {
        return mPaymentUiService.getSelectedPaymentApp();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public List<PaymentApp> getPaymentApps() {
        return mPaymentUiService.getPaymentApps();
    }

  // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAnyCompleteApp() {
        return mPaymentUiService.hasAnyCompleteAppSuggestion();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onSpecValidated(PaymentRequestSpec spec) {
        mSpec = spec;
        mPaymentUiService.initialize(mSpec.getPaymentDetails());
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean disconnectIfExtraValidationFails(
            WebContents webContents,
            Map<String, PaymentMethodData> methodData,
            PaymentDetails details,
            PaymentOptions options) {
        assert methodData != null;
        assert details != null;

        if (!parseAndValidateDetailsFurtherIfNeeded(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return true;
        }
        return false;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String showOrSkipAppSelector(
            boolean isShowWaitingForUpdatedDetails,
            PaymentItem total,
            boolean shouldSkipAppSelector) {
      // TODO(jfernandez): Implement this method properly.
      mHasSkippedAppSelector = true;
      return null;
    }

    private void dimBackgroundIfNotPaymentHandler(PaymentApp selectedApp) {
        if (selectedApp != null
                && selectedApp.getPaymentAppType() == PaymentAppType.SERVICE_WORKER_APP) {
            // As bottom-sheet itself has dimming effect, dimming PR is unnecessary for the
            // bottom-sheet PH. For now, service worker based payment apps are the only ones that
            // can open the bottom-sheet.
            return;
        }
        mPaymentUiService.dimBackground();
    }

  @Override
    public String onShowCalledAndAppsQueriedAndDetailsFinalized() {
        Log.d(TAG, "onShowCalledAndAppsQueriedAndDetailsFinalized");
        WindowAndroid windowAndroid = mDelegate.getWindowAndroid(mRenderFrameHost);
        if (windowAndroid == null) return ErrorStrings.WINDOW_NOT_FOUND;
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) return ErrorStrings.CONTEXT_NOT_FOUND;

        // If we are skipping showing the app selector UI, we should call into the payment app
        // immediately after we determine the apps are ready and UI is shown.
        if (mHasSkippedAppSelector) {
            assert !mPaymentUiService.getPaymentApps().isEmpty();
            PaymentApp selectedApp = mPaymentUiService.getSelectedPaymentApp();
            dimBackgroundIfNotPaymentHandler(selectedApp);
            mJourneyLogger.setSkippedShow();
            invokePaymentApp(
                    /* selectedShippingAddress= */ null,
                    /* selectedShippingOption= */ null,
                    selectedApp);
        } else {
          mPaymentUiService.createShippingSectionIfNeeded(context);
        }
        return null;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public @Nullable WebContents openPaymentHandlerWindow(GURL url, long ukmSourceId) {
        Log.d(TAG, "openPaymentHandlerWindow");
        @Nullable
        WebContents paymentHandlerWebContents = mPaymentUiService.showPaymentHandlerUI(url);
        if (paymentHandlerWebContents != null) {
          Log.d(TAG, "No PaymentHandler webcontent !");
          ServiceWorkerPaymentAppBridge.onOpeningPaymentAppWindow(
                /* paymentRequestWebContents= */ mWebContents,
                /* paymentHandlerWebContents= */ paymentHandlerWebContents);

          // UKM for payment app origin should get recorded only when the origin of the invoked
            // payment app is shown to the user.
            mJourneyLogger.setPaymentAppUkmSourceId(ukmSourceId);
        }
        return paymentHandlerWebContents;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsUpdated(
            PaymentDetails details, boolean hasNotifiedInvokedPaymentApp) {
        mPaymentUiService.updateDetailsOnPaymentRequestUI(details);

        if (hasNotifiedInvokedPaymentApp) return;

        mPaymentUiService.showShippingAddressErrorIfApplicable(details.error);
        mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String continueShowWithUpdatedDetails(
            PaymentDetails details, boolean isFinishedQueryingPaymentApps) {
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) return ErrorStrings.CONTEXT_NOT_FOUND;

        mPaymentUiService.updateDetailsOnPaymentRequestUI(details);

        if (isFinishedQueryingPaymentApps && !mHasSkippedAppSelector) {
            mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
        }
        return null;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError) {
        mPaymentUiService.showShippingAddressErrorIfApplicable(selectedShippingOptionError);
        mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void complete(int result, Runnable onCompleteHandled) {
      // TODO(jfernandez): Store the payment request status in the preferences store.
      mPaymentUiService.onPaymentRequestComplete(result, onCompleteHandled);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onRetry(PaymentValidationErrors errors) {
        mWasRetryCalled = true;
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) {
            disconnectFromClientWithDebugMessage(ErrorStrings.CONTEXT_NOT_FOUND);
            return;
        }
        mPaymentUiService.onRetry(context, errors);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        if (mPaymentRequestService != null) {
            mPaymentRequestService.close();
            mPaymentRequestService = null;
        }

        mPaymentUiService.close();

        if (mPaymentHandlerHost != null) {
          mPaymentHandlerHost.destroy();
          mPaymentHandlerHost = null;
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean onPaymentAppCreated(PaymentApp paymentApp) {
        paymentApp.setHaveRequestedAutofillData(mPaymentUiService.haveRequestedAutofillData());
        return true;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps) {
        Log.d(TAG, "notifyPaymentUiOfPendingApps");
        mPaymentUiService.setPaymentApps(pendingApps);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAvailableApps() {
        Log.d(TAG, "hasAvailableApps");
        return mPaymentUiService.hasAvailableApps();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsReady() {
        // Showing the app selector UI if we were previously skipping it so the loading
        // spinner shows up until the merchant notifies that payment was completed.
        if (mHasSkippedAppSelector) {
            mPaymentUiService.showProcessingMessageAfterUiSkip();
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasSkippedAppSelector() {
        return mHasSkippedAppSelector;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void showAppSelectorAfterPaymentAppInvokeFailed() {
        mPaymentUiService.onPayButtonProcessingCancelled();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean isShippingSectionVisible() {
        // TODO(jfernandez): Implement this method.
        return false;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean isContactSectionVisible() {
        // TODO(jfernandez): Implement this method.
        return false;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean wasRetryCalled() {
        return mWasRetryCalled;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean invokePaymentApp(
            EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption,
            PaymentApp selectedPaymentApp) {

      if (mPaymentRequestService == null || mSpec == null || mSpec.isDestroyed()) return false;
      selectedPaymentApp.setPaymentHandlerHost(getPaymentHandlerHost());
        PaymentResponseHelperInterface paymentResponseHelper =
                new WolvicPaymentResponseHelper(
                        selectedPaymentApp,
                        mSpec.getPaymentOptions());
        mPaymentRequestService.invokePaymentApp(selectedPaymentApp, paymentResponseHelper);
        return true;
    }

    private PaymentHandlerHost getPaymentHandlerHost() {
        if (mPaymentHandlerHost == null) {
            mPaymentHandlerHost =
                    mDelegate.createPaymentHandlerHost(
                            mWebContents, /* listener= */ mPaymentRequestService);
        }
        return mPaymentHandlerHost;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public void onUiAborted(@AbortReason int reason, String debugMessage) {
        mJourneyLogger.setAborted(reason);
        disconnectFromClientWithDebugMessage(debugMessage);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage) {
        if (mPaymentRequestService != null) {
            mPaymentRequestService.disconnectFromClientWithDebugMessage(
                    debugMessage, PaymentErrorReason.USER_CANCEL);
        }
        close();
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public @Nullable Context getContext() {
        return mDelegate.getContext(mRenderFrameHost);
    }
}
