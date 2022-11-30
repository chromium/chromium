// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.translate.TranslateMenu;

/**
 * Bridge for TranslateCompactInfoBar test methods invoked from native.
 */
@JNINamespace("weblayer")
public class TranslateTestBridge {
    TranslateTestBridge() {}

    // Selects the tab corresponding to |actionType| to simulate the user pressing on this tab.
    @CalledByNative
    private static void selectTab(TranslateCompactInfoBar infobar, int actionType) {
        infobar.selectTabForTesting(actionType);
    }

    @CalledByNative
    // Simulates a click of the overflow menu item for "never translate this language."
    private static void clickNeverTranslateLanguageMenuItem(TranslateCompactInfoBar infobar) {
        infobar.onOverflowMenuItemClicked(TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE);
    }

    @CalledByNative
    // Simulates a click of the overflow menu item for "never translate this site."
    private static void clickNeverTranslateSiteMenuItem(TranslateCompactInfoBar infobar) {
        infobar.onOverflowMenuItemClicked(TranslateMenu.ID_OVERFLOW_NEVER_SITE);
    }
}
