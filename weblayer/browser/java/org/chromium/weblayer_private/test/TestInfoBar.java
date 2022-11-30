// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarCompactLayout;
import org.chromium.content_public.browser.WebContents;
import org.chromium.weblayer_private.TabImpl;

/**
 * A test infobar.
 */
@JNINamespace("weblayer")
public class TestInfoBar extends InfoBar {
    @VisibleForTesting
    public TestInfoBar() {
        super(0, 0, null, null);
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        new InfoBarCompactLayout.MessageBuilder(layout)
                .withText("I am a compact infobar")
                .buildAndInsert();
    }

    @CalledByNative
    private static TestInfoBar create() {
        return new TestInfoBar();
    }

    public static void show(TabImpl tab) {
        TestInfoBarJni.get().show(tab.getWebContents());
    }

    @NativeMethods
    interface Natives {
        void show(WebContents webContents);
    }
}
