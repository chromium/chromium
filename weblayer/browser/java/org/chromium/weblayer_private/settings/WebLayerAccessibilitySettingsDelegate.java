// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.settings;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.accessibility.AccessibilitySettingsDelegate;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.weblayer_private.ProfileImpl;

/** The WebLayer implementation of AccessibilitySettingsDelegate. */
public class WebLayerAccessibilitySettingsDelegate implements AccessibilitySettingsDelegate {
    private ProfileImpl mProfile;

    public WebLayerAccessibilitySettingsDelegate(ProfileImpl profile) {
        mProfile = profile;
    }

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mProfile;
    }

    @Override
    public BooleanPreferenceDelegate getAccessibilityTabSwitcherDelegate() {
        return null;
    }

    @Override
    public BooleanPreferenceDelegate getReaderForAccessibilityDelegate() {
        return null;
    }

    @Override
    public void addExtraPreferences(PreferenceFragmentCompat fragment) {}

    @Override
    public boolean showPageZoomSettingsUI() {
        return false;
    }
}
