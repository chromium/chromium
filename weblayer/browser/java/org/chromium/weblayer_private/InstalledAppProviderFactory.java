// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.components.installedapp.InstalledAppProviderImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.services.service_manager.InterfaceFactory;

/** Factory to create instances of the InstalledAppProvider Mojo service. */
public class InstalledAppProviderFactory implements InterfaceFactory<InstalledAppProvider> {
    private final RenderFrameHost mRenderFrameHost;

    public InstalledAppProviderFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public InstalledAppProvider createImpl() {
        TabImpl tab =
                TabImpl.fromWebContents(WebContentsStatics.fromRenderFrameHost(mRenderFrameHost));
        if (tab == null) return null;
        return new InstalledAppProviderImpl(
                tab.getProfile(), mRenderFrameHost, (unusedA, unusedB, unusedC) -> false);
    }
}
