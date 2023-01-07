// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.components.webauthn.AuthenticatorFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceRegistry;
import org.chromium.weblayer_private.payments.WebLayerPaymentRequestFactory;
import org.chromium.webshare.mojom.ShareService;

/**
 * Registers Java implementations of mojo interfaces.
 */
class MojoInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addWebContentsRegistrar(new WebContentsInterfaceRegistrar());
        InterfaceRegistrar.Registry.addRenderFrameHostRegistrar(
                new RenderFrameHostInterfaceRegistrar());
    }

    private static class WebContentsInterfaceRegistrar implements InterfaceRegistrar<WebContents> {
        @Override
        public void registerInterfaces(InterfaceRegistry registry, final WebContents webContents) {
            registry.addInterface(ShareService.MANAGER, new WebShareServiceFactory(webContents));
        }
    }

    private static class RenderFrameHostInterfaceRegistrar
            implements InterfaceRegistrar<RenderFrameHost> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry, final RenderFrameHost renderFrameHost) {
            registry.addInterface(Authenticator.MANAGER, new AuthenticatorFactory(renderFrameHost));
            registry.addInterface(
                    InstalledAppProvider.MANAGER, new InstalledAppProviderFactory(renderFrameHost));
            registry.addInterface(
                    PaymentRequest.MANAGER, new WebLayerPaymentRequestFactory(renderFrameHost));
        }
    }
}
