package org.chromium.wolvic.mojo;

import android.util.Log;

import org.jni_zero.CalledByNative;

import org.chromium.wolvic.payments.WolvicPaymentRequestFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceRegistry;
import org.chromium.wolvic.installedapp.WolvicInstalledAppProviderFactory;

/** Registers mojo interface implementations exposed to C++ code at the Wolvic layer. */
class WolvicInterfaceRegistrar {
    private static final String TAG = "WolvicInterfaceRegistrar";

    @CalledByNative
    private static void registerMojoInterfaces() {
        Log.w(TAG, "registerMojoInterfaces() called !");
        InterfaceRegistrar.Registry.addRenderFrameHostRegistrar(
                new WolvicRenderFrameHostInterfaceRegistrar());
    }

    private static class WolvicRenderFrameHostInterfaceRegistrar
            implements InterfaceRegistrar<RenderFrameHost> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry, final RenderFrameHost renderFrameHost) {
            Log.w(TAG, "registerInterfaces() called !");

            registry.addInterface(
                    InstalledAppProvider.MANAGER,
                    new WolvicInstalledAppProviderFactory(renderFrameHost));
            registry.addInterface(
                    PaymentRequest.MANAGER, new WolvicPaymentRequestFactory(renderFrameHost));
        }
    }
}
