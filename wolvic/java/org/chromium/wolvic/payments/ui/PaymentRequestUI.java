package org.chromium.wolvic.payments.ui;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

import org.chromium.content_public.browser.WebContents;

import org.chromium.wolvic.Tab;

/** The PaymentRequest UI. */
public class PaymentRequestUI extends Tab {

    /** The interface to be implemented by the consumer of the PaymentRequest UI. */
    public interface Client {

        /**
         * Called when the user dismisses the UI via clicking outsude the modal dialog.
         */
        void onDismiss();
    }

    private Client mClient;

    public PaymentRequestUI(
            @NonNull Context context,
            @Nullable WebContents webContents,
            @Nullable Client client) {
      super(context, false, webContents);
      mClient = client;
    }

    @Override
    public void destroy() {
      super.destroy();
      if (mClient != null)
        mClient.onDismiss();
    }
}
