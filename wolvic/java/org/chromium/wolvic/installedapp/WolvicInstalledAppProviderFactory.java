// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic.installedapp;

import org.chromium.components.installedapp.InstalledAppProviderImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.wolvic.WolvicBrowserContext;

/** Factory to create instances of the InstalledAppProvider Mojo service. */
public class WolvicInstalledAppProviderFactory implements InterfaceFactory<InstalledAppProvider> {
    private final RenderFrameHost mRenderFrameHost;

    public WolvicInstalledAppProviderFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public InstalledAppProvider createImpl() {
        return new InstalledAppProviderImpl(
                WolvicBrowserContext.fromWebContents(WebContentsStatics.fromRenderFrameHost(mRenderFrameHost)),
                mRenderFrameHost, null);
    }
}
