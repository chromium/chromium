// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.media;

import android.content.Intent;

import androidx.fragment.app.FragmentManager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.content_public.browser.WebContents;
import org.chromium.weblayer_private.IntentUtils;
import org.chromium.weblayer_private.TabImpl;

/** Provides WebLayer-specific behavior for Media Router. */
@JNINamespace("weblayer")
public class MediaRouterClientImpl extends MediaRouterClient {
    private MediaRouterClientImpl() {}

    @Override
    public int getTabId(WebContents webContents) {
        TabImpl tab = TabImpl.fromWebContents(webContents);
        return tab == null ? -1 : tab.getId();
    }

    @Override
    public Intent createBringTabToFrontIntent(int tabId) {
        return IntentUtils.createBringTabToFrontIntent(tabId);
    }

    @Override
    public void showNotification(MediaNotificationInfo notificationInfo) {
        // TODO: implement.
    }

    @Override
    public FragmentManager getSupportFragmentManager(WebContents initiator) {
        return TabImpl.fromWebContents(initiator)
                .getBrowser()
                .createMediaRouteDialogFragment()
                .getSupportFragmentManager();
    }

    @CalledByNative
    public static void initialize() {
        if (MediaRouterClient.getInstance() != null) return;

        MediaRouterClient.setInstance(new MediaRouterClientImpl());
    }
}
