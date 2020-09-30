// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory.Type;
import org.chromium.components.browser_ui.site_settings.SiteSettingsClient;
import org.chromium.components.browser_ui.site_settings.SiteSettingsHelpClient;
import org.chromium.components.browser_ui.site_settings.WebappSettingsClient;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.page_info.PageInfoFeatureList;

import java.util.Collections;
import java.util.Set;

/**
 * A SiteSettingsClient instance that contains WebLayer-specific Site Settings logic.
 */
public class WebLayerSiteSettingsClient implements SiteSettingsClient, ManagedPreferenceDelegate,
                                                   SiteSettingsHelpClient, WebappSettingsClient {
    private final BrowserContextHandle mBrowserContextHandle;

    public WebLayerSiteSettingsClient(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;
    }

    // SiteSettingsClient implementation:

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mBrowserContextHandle;
    }

    @Override
    public ManagedPreferenceDelegate getManagedPreferenceDelegate() {
        return this;
    }

    @Override
    public SiteSettingsHelpClient getSiteSettingsHelpClient() {
        return this;
    }

    @Override
    public WebappSettingsClient getWebappSettingsClient() {
        return this;
    }

    @Override
    public void getFaviconImageForURL(String faviconUrl, Callback<Bitmap> callback) {
        // We don't currently support favicons on WebLayer.
        callback.onResult(null);
    }

    @Override
    public boolean isCategoryVisible(@Type int type) {
        return type == Type.ALL_SITES || type == Type.AUTOMATIC_DOWNLOADS || type == Type.CAMERA
                || type == Type.COOKIES || type == Type.DEVICE_LOCATION || type == Type.JAVASCRIPT
                || type == Type.MICROPHONE || type == Type.POPUPS || type == Type.PROTECTED_MEDIA
                || type == Type.SOUND || type == Type.USE_STORAGE;
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
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
        if (WebLayerImpl.isLocationPermissionManaged(origin)
                && type == ContentSettingsType.GEOLOCATION) {
            return WebLayerImpl.getClientApplicationName();
        }

        return null;
    }

    @Override
    @Nullable
    public String getDelegatePackageNameForOrigin(Origin origin, @ContentSettingsType int type) {
        if (WebLayerImpl.isLocationPermissionManaged(origin)
                && type == ContentSettingsType.GEOLOCATION) {
            return ContextUtils.getApplicationContext().getPackageName();
        }

        return null;
    }

    // TODO(crbug.com/1133798): Remove this when the feature flag is no longer used.
    @Override
    public boolean isPageInfoV2Enabled() {
        return PageInfoFeatureList.isEnabled(PageInfoFeatureList.PAGE_INFO_V2);
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

    // SiteSettingsHelpClient implementation:
    // A no-op since WebLayer doesn't have help pages.

    @Override
    public boolean isHelpAndFeedbackEnabled() {
        return false;
    }

    @Override
    public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {}

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {}

    // WebappSettingsClient implementation:
    // A no-op since WebLayer doesn't support webapps.

    @Override
    public Set<String> getOriginsWithInstalledApp() {
        return Collections.EMPTY_SET;
    }

    @Override
    public Set<String> getAllDelegatedNotificationOrigins() {
        return Collections.EMPTY_SET;
    }
}
