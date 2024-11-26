// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic.payments.ui;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.components.autofill.Completable;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentHandlerNavigationThrottle;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.components.payments.Section;

import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentValidationErrors;

import org.chromium.wolvic.WolvicWebContentsFactory;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class WolvicPaymentUiService {

    /** Limit in the number of suggested items in a section. */
    /* package */ static final int SUGGESTIONS_LIMIT = 4;

    private final boolean mIsOffTheRecord;
    private final Delegate mDelegate;
    private final WebContents mWebContents;
    private final String mTopLevelOriginFormattedForDisplay;
    private final String mMerchantName;
    private final Map<String, CurrencyFormatter> mCurrencyFormatterMap;
    private final PaymentRequestParams mParams;
    private final JourneyLogger mJourneyLogger;

    private boolean mHasInitialized;
    private boolean mHasClosed;
    private boolean mHaveRequestedAutofillData = true;

    private List<PaymentApp> mPaymentApps;

    private static final String TAG = "WolvicPaymentUiService";

    /** The delegate of this class. */
    // TODO(jfernandez): Define the a complete delegate
    public interface Delegate {
        /**
         * @return Whether {@link ChromePaymentRequestService#onRetry} has been
         *         called.
         */
        boolean wasRetryCalled();

        /**
         * Invokes the selected payment app.
         * @param selectedShippingAddress the shipping address selected from the payment request UI.
         * @param selectedShippingOption The shipping option selected from the payment request UI.
         * @param selectedPaymentApp The selected payment app.
         * @return Whether the spinner should be displayed. Autofill cards show a CVC prompt, so the
         *         spinner is hidden in that case. Other payment apps typically show a spinner.
         */
        boolean invokePaymentApp(
                EditableOption selectedShippingAddress,
                EditableOption selectedShippingOption,
                PaymentApp selectedPaymentApp);

       /**
         * Invoked when the UI service has been aborted.
         * @param reason The reason for the aborting, as defined by {@link AbortReason}.
         * @param debugMessage The debug message for the aborting.
         */
        void onUiAborted(@AbortReason int reason, String debugMessage);

        /**
         * @return The context of the current activity, can be null when WebContents has been
         *         destroyed, the activity is gone, the window is closed, etc.
         */
        @Nullable
        Context getContext();
    }

    /**
     * Create PaymentUiService.
     * @param delegate The delegate of this instance.
     * @param webContents The WebContents of the merchant page.
     * @param isOffTheRecord Whether merchant page is in an isOffTheRecord tab.
     * @param journeyLogger The logger of the user journey.
     * @param topLevelOrigin The last committed url of webContents.
     */
    public WolvicPaymentUiService(
            Delegate delegate,
            PaymentRequestParams params,
            WebContents webContents,
            boolean isOffTheRecord,
            JourneyLogger journeyLogger,
            String topLevelOrigin) {
        assert !params.hasClosed();
        mDelegate = delegate;
        mParams = params;

        mJourneyLogger = journeyLogger;
        mWebContents = webContents;
        mTopLevelOriginFormattedForDisplay = topLevelOrigin;
        mMerchantName = webContents.getTitle();

        mCurrencyFormatterMap = new HashMap<>();
        mIsOffTheRecord = isOffTheRecord;
    }

    /**
     * Creates the shipping section for the app selector UI if needed. This method should be called
     * when UI has been built and payment details has been finalized.
     * @param context The activity context.
     */
    public void createShippingSectionIfNeeded(Context context) {
      // TODO(jfernandez): Implement this method.
    }

    /** @return The payment apps. */
    public List<PaymentApp> getPaymentApps() {
      // TODO(jfernandez): Implement this method.
      return mPaymentApps;
    }

    /**
     * Whether the payment apps includes at least one that is "complete" which is defined
     * by {@link PaymentApp#isComplete()}. This method can be called only after
     * {@link #setPaymentApps}.
     * @return The result.
     */
    public boolean hasAnyCompleteAppSuggestion() {
        List<PaymentApp> apps = getPaymentApps();
        return !apps.isEmpty() && apps.get(0).isComplete();
    }

    /**
     * Returns the selected payment app, if any.
     * @return The selected payment app or null if none selected.
     */
    public @Nullable PaymentApp getSelectedPaymentApp() {
      // TODO(jfernandez): Implement this method.
      List<PaymentApp> apps = getPaymentApps();
      return apps.isEmpty() ? null : apps.get(0);
    }

    /**
     * Loads the payment apps into the app selector UI (aka, PaymentRequest UI).
     * @param apps The payment apps to be loaded into the app selector UI.
     */
     public void setPaymentApps(List<PaymentApp> apps) {
       // TODO(jfernandez): Implement this method.
       Log.d(TAG, "setPaymentApps");
       mPaymentApps = new ArrayList<>(apps);
     }

    /** @return Whether at least one payment app is available. */
    public boolean hasAvailableApps() {
        Log.d(TAG, "hasAvailableApps");
        assert mHasInitialized;
        // TODO(jfernandez): Implement this method.
        return !mPaymentApps.isEmpty();
    }

    /**
     *  Shows the processing message after payment details have been loaded in the case the
     *  app selector UI has been skipped. Precondition: isPaymentRequestUiAlive() needs to be
     *  true for the method to take effect.
     */
    public void showProcessingMessageAfterUiSkip() {
        // TODO(jfernandez): Implement this method.
    }

    /**
     * Called when user cancelled out of the UI that was shown after they clicked [PAY] button.
     * Precondition: isPaymentRequestUiAlive() needs to be true for the method to take effect.
     */
    public void onPayButtonProcessingCancelled() {
        // TODO(jfernandez): Implement this method.
    }

    /** @return Whether PaymentRequestUI has requested autofill data. */
    public boolean haveRequestedAutofillData() {
        return mHaveRequestedAutofillData;
    }

    /**
     * Called when the merchant calls complete() to complete the payment request.
     * @param result The completion status of the payment request, defined in {@link
     *         PaymentComplete}, provided by the merchant with
     * PaymentResponse.complete(paymentResult).
     * @param onUiCompleted The function called when the opened UI has handled the completion.
     */
    public void onPaymentRequestComplete(int result, Runnable onUiCompleted) {
        // Update records of the used payment app for sorting payment apps next time.
        PaymentApp paymentApp = getSelectedPaymentApp();
        assert paymentApp != null;
        String selectedPaymentMethod = paymentApp.getIdentifier();

        // TODO(jfernandez): Manage counters in Chrome preferenes store

        // TODO(https://crbug.com/1188895): The caller should execute the function at onUiCompleted
        // directly instead of passing the Runnable here, because there are no asynchronous
        // operations in this code path.
        onUiCompleted.run();
    }

    /**
     * Called after {@link PaymentRequest#retry} is invoked.
     * @param context The context of the main activity.
     * @param errors The payment validation errors.
     */
    public void onRetry(Context context, PaymentValidationErrors errors) {
      // TODO(jfernandez): Implement this method.
    }

    /**
     * Initializes the payment UI service.
     * @param details The PaymentDetails provided by the merchant.
     */
    public void initialize(PaymentDetails details) {
      // TODO(jfernandez): Implement a proprer initialization logic here
        assert !mParams.hasClosed();
        mHasInitialized = true;
    }

    /**
     * Create and show the (BottomSheet) PaymentHandler UI.
     *
     * @param url The URL of the payment app.
     * @return The WebContents of the payment handler that's just opened when the opening is
     *     successful; null if failed.
     */
    public @Nullable WebContents showPaymentHandlerUI(GURL url) {
      WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
      if (windowAndroid == null) return null;
      Activity activity = windowAndroid.getActivity().get();
      if (activity == null) return null;

      WebContents paymentHandlerWebContents = createWebContents(mIsOffTheRecord);
      if (paymentHandlerWebContents == null)
        return null;
      PaymentHandlerNavigationThrottle.markPaymentHandlerWebContents(paymentHandlerWebContents);
      ContentView webContentView =
             ContentView.createContentView(
                     activity, /* eventOffsetHandler= */ null, paymentHandlerWebContents);
      paymentHandlerWebContents.initialize(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(webContentView),
                webContentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());
      paymentHandlerWebContents
               .getNavigationController()
               .loadUrl(new LoadUrlParams(url.getSpec()));

      mWebContents.notifyOnCreateNewPaymentHandler(paymentHandlerWebContents);

      return paymentHandlerWebContents;
    }

    private @Nullable WebContents createWebContents(boolean isOffTheRecord) {
      return WolvicWebContentsFactory.createWebContents(isOffTheRecord);
    }

    /**
     * Update the details related fields on the PaymentRequest UI.
     * @param details The details whose information is used for the update.
     */
    public void updateDetailsOnPaymentRequestUI(PaymentDetails details) {
      //TODO(jfernandez): Implements this method.
    }

    /**
     * Update Payment Request UI with the update event's information and enable the UI. This method
     * should be called when the user interface is disabled with a spinner being displayed. The
     * user is unable to interact with the user interface until this method is called.
     */
    public void enableAndUpdatePaymentRequestUIWithPaymentInfo() {
      //TODO(jfernandez): Implements this method.
    }

   /**
     * Shows the shipping address error if any.
     * @param error The shipping address error, can be null.
     */
    public void showShippingAddressErrorIfApplicable(@Nullable String error) {
      //TODO(jfernandez): Implements this method.
    }

    /**
     * Shows the app selector UI. Precondition: isPaymentRequestUiAlive() needs to be true for
     * the method to take effect.
     * @param isShowWaitingForUpdatedDetails
     *        Whether showing payment app or the app selector is blocked on the updated payment
     *        details.
     */
    public void showAppSelector(boolean isShowWaitingForUpdatedDetails) {
      //TODO(jfernandez): Implements this method.
    }

    /**
     * Dims the background of the payment UIs. Precondition: isPaymentRequestUiAlive() needs to be
     * true for the method to take effect.
     */
    public void dimBackground() {
      //TODO(jfernandez): Implements this method.
    }

    /** Close the instance. Do not use this instance any more after calling this method. */
    public void close() {
        assert !mHasClosed;
        mHasClosed = true;

        // TODO(jfernandez): Implement properly the "close" logic.

    }
}
