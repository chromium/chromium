// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.autofill_assistant;

import org.chromium.base.ObserverList;
import org.chromium.components.autofill_assistant.AssistantTabChangeObserver;
import org.chromium.content_public.browser.WebContents;

/**
 * Notifies of changes to a WebLayer tab.
 * The functions onContentChanged and onWebContentsSwapped are never called on WebLayer since the
 * tab never changes WebContents.
 */
public class WebLayerAssistantTabChangeObserver {
    private final ObserverList<AssistantTabChangeObserver> mTabChangeObservers =
            new ObserverList<AssistantTabChangeObserver>();

    // TODO(b/222671580): onObservingDifferentTab
    // TODO(b/222671580): onInteractabilityChanged

    public void addObserver(AssistantTabChangeObserver tabChangeObserver) {
        mTabChangeObservers.addObserver(tabChangeObserver);
    }

    public void removeObserver(AssistantTabChangeObserver tabChangeObserver) {
        mTabChangeObservers.removeObserver(tabChangeObserver);
    }

    public void onBrowserAttachmentChanged(WebContents webContents) {
        for (AssistantTabChangeObserver tabChangeObserver : mTabChangeObservers) {
            tabChangeObserver.onActivityAttachmentChanged(
                    webContents, webContents.getTopLevelNativeWindow());
        }
    }

    public void onTabDestroyed(WebContents webContents) {
        for (AssistantTabChangeObserver tabChangeObserver : mTabChangeObservers) {
            tabChangeObserver.onDestroyed(webContents);
        }
    }
}
