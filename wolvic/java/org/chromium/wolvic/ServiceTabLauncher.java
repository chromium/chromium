package org.chromium.wolvic;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/**
 * Tab Launcher to be used to launch new tabs from background Android Services,
 * when it is not known whether an activity is available. It will send an intent to launch the
 * activity.
 *
 * URLs within the scope of a recently launched standalone-capable web app on the Android home
 * screen are launched in the standalone web app frame.
 */
public class ServiceTabLauncher {
    private static final String TAG = "ServiceTabLauncher";

    /**
     * Launches the browser activity and launches a tab for |url|.
     *
     * @param requestId      Id of the request for launching this tab.
     * @param incognito      Whether the tab should be launched in incognito mode.
     * @param url            The URL which should be launched in a tab.
     * @param disposition    The disposition requested by the navigation source.
     * @param referrerUrl    URL of the referrer which is opening the page.
     * @param referrerPolicy The referrer policy to consider when applying the referrer.
     * @param extraHeaders   Extra headers to apply when requesting the tab's URL.
     * @param postData       Post-data to include in the tab URL's request body.
     */
    @CalledByNative
    public static void launchTab(
            final int requestId,
            boolean incognito,
            GURL url,
            int disposition,
            String referrerUrl,
            int referrerPolicy,
            String extraHeaders,
            ResourceRequestBody postData) {
      // TODO(jfernandez): We only support launching popps for now.
      if (disposition != WindowOpenDisposition.NEW_POPUP) {
        Log.e(TAG, "Wolvic only supports launching popup windows !");
        return;
      }

      // Open popup window in custom tab.
      // Note that this is used by PaymentRequestEvent.openWindow().
      WebContents paymentHandlerWebContent =
        PaymentRequestService.openPaymentHandlerWindow(url);
      if (paymentHandlerWebContent != null) {
        onWebContentsForRequestAvailable(requestId, paymentHandlerWebContent);
      } else {
        PostTask.postTask(
                          TaskTraits.UI_DEFAULT,
                          () -> onWebContentsForRequestAvailable(requestId, null));
      }
    }

    /**
     * To be called by the activity when the WebContents for |requestId| has been created, or has
     * been recycled from previous use. The |webContents| must not yet have started provisional
     * load for the main frame.
     * The |webContents| could be null if the request is failed.
     *
     * @param requestId Id of the tab launching request which has been fulfilled.
     * @param webContents The WebContents instance associated with this request.
     */
    public static void onWebContentsForRequestAvailable(
            int requestId, @Nullable WebContents webContents) {
        ServiceTabLauncherJni.get().onWebContentsForRequestAvailable(requestId, webContents);
    }

    @NativeMethods
    public interface Natives {
        void onWebContentsForRequestAvailable(int requestId, WebContents webContents);
    }
}
