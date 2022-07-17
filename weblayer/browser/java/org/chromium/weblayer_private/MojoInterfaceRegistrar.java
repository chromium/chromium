// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.components.webauthn.AuthenticatorFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.services.service_manager.InterfaceRegistry;

/**
 * Registers Java implementations of mojo interfaces.
 */
class MojoInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addRenderFrameHostRegistrar(
                new RenderFrameHostInterfaceRegistrar());
    }

    private static class RenderFrameHostInterfaceRegistrar
            implements InterfaceRegistrar<RenderFrameHost> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry, final RenderFrameHost renderFrameHost) {
            registry.addInterface(Authenticator.MANAGER, new AuthenticatorFactory(renderFrameHost));
        }
    }
}
