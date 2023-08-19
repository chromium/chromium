// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.settings;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory.Type;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.WebLayerImpl;

import java.util.Collections;
import java.util.Set;

/**
 * A SiteSettingsDelegate instance that contains WebLayer-specific Site Settings logic.
 */
public class WebLayerSiteSettingsDelegate
        implements SiteSettingsDelegate, ManagedPreferenceDelegate {
    private final BrowserContextHandle mBrowserContextHandle;

    public WebLayerSiteSettingsDelegate(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;
    }

    // SiteSettingsDelegate implementation:

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mBrowserContextHandle;
    }

    @Override
    public ManagedPreferenceDelegate getManagedPreferenceDelegate() {
        return this;
    }

    @Override
    public void resetZoomLevel(String host) {}

    @Override
    public void getFaviconImageForURL(GURL faviconUrl, Callback<Drawable> callback) {
        // We don't currently support favicons on WebLayer.
        callback.onResult(null);
    }

    @Override
    public boolean isCategoryVisible(@Type int type) {
        return type == Type.ADS || type == Type.ALL_SITES || type == Type.AUTOMATIC_DOWNLOADS
                || type == Type.BACKGROUND_SYNC || type == Type.CAMERA || type == Type.COOKIES
                || type == Type.DEVICE_LOCATION || type == Type.JAVASCRIPT
                || type == Type.MICROPHONE || type == Type.POPUPS || type == Type.PROTECTED_MEDIA
                || type == Type.SOUND || type == Type.USE_STORAGE || type == Type.ZOOM;
    }

    @Override
    public boolean isIncognitoModeEnabled() {
        return true;
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return false;
    }

    @Override
    public boolean isPrivacySandboxFirstPartySetsUIFeatureEnabled() {
        return false;
    }

    @Override
    public boolean isPrivacySandboxSettings4Enabled() {
        return false;
    }

    @Override
    public boolean isUserBypassUIEnabled() {
        return false;
    }

    @Override
    public String getChannelIdForOrigin(String origin) {
        return null;
    }

    @Override
    public String getAppName() {
        return WebLayerImpl.getClientApplicationName();
    }

    @Override
    @Nullable
    public String getDelegateAppNameForOrigin(Origin origin, @ContentSettingsType int type) {
        return null;
    }

    @Override
    @Nullable
    public String getDelegatePackageNameForOrigin(Origin origin, @ContentSettingsType int type) {
        return null;
    }

    // ManagedPrefrenceDelegate implementation:
    // A no-op because WebLayer doesn't support managed preferences.

    @Override
    public boolean isPreferenceControlledByPolicy(Preference preference) {
        return false;
    }

    @Override
    public boolean isPreferenceControlledByCustodian(Preference preference) {
        return false;
    }

    @Override
    public boolean doesProfileHaveMultipleCustodians() {
        return false;
    }

    @Override
    public @LayoutRes int defaultPreferenceLayoutResource() {
        // WebLayer uses Android's default Preference layout.
        return 0;
    }

    @Override
    public boolean isHelpAndFeedbackEnabled() {
        return false;
    }

    @Override
    public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {}

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {}

    @Override
    public Set<String> getOriginsWithInstalledApp() {
        return Collections.EMPTY_SET;
    }

    @Override
    public Set<String> getAllDelegatedNotificationOrigins() {
        return Collections.EMPTY_SET;
    }

    @Override
    public void maybeDisplayPrivacySandboxSnackbar() {}

    @Override
    public void dismissPrivacySandboxSnackbar() {}

    @Override
    public boolean isFirstPartySetsDataAccessEnabled() {
        return false;
    }

    @Override
    public boolean isFirstPartySetsDataAccessManaged() {
        return false;
    }

    @Override
    public boolean isPartOfManagedFirstPartySet(String origin) {
        return false;
    }

    @Override
    public void setFirstPartySetsDataAccessEnabled(boolean enabled) {}

    @Override
    public String getFirstPartySetOwner(String memberOrigin) {
        return null;
    }

    @Override
    public boolean canLaunchClearBrowsingDataDialog() {
        return false;
    }

    @Override
    public void launchClearBrowsingDataDialog(Activity currentActivity) {}

    @Override
    public void notifyRequestDesktopSiteSettingsPageOpened() {}

    @Override
    public void onDestroyView() {}
}
