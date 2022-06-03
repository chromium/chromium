// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.webshare.ShareServiceImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.webshare.mojom.ShareService;

/**
 * Factory that creates instances of ShareService.
 */
public class WebShareServiceFactory implements InterfaceFactory<ShareService> {
    private final WebContents mWebContents;

    public WebShareServiceFactory(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public ShareService createImpl() {
        ShareServiceImpl.WebShareDelegate delegate = new ShareServiceImpl.WebShareDelegate() {
            @Override
            public boolean canShare() {
                return mWebContents.getTopLevelNativeWindow().getActivity() != null;
            }

            @Override
            public void share(ShareParams params) {
                ShareHelper.shareWithUi(params);
            }
        };

        return new ShareServiceImpl(mWebContents, delegate);
    }
}
