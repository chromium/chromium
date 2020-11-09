// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsClient;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.SiteSettingsIntentHelper;

/**
 * WebLayer's customization of PageInfoControllerDelegate.
 */
public class PageInfoControllerDelegateImpl extends PageInfoControllerDelegate {
    private final Context mContext;
    private final WebContents mWebContents;
    private final ProfileImpl mProfile;

    static PageInfoControllerDelegateImpl create(WebContents webContents) {
        TabImpl tab = TabImpl.fromWebContents(webContents);
        assert tab != null;
        return new PageInfoControllerDelegateImpl(tab.getBrowser().getContext(), webContents,
                tab.getProfile(), tab.getBrowser().getWindowAndroid()::getModalDialogManager);
    }

    private PageInfoControllerDelegateImpl(Context context, WebContents webContents,
            ProfileImpl profile, Supplier<ModalDialogManager> modalDialogManager) {
        super(modalDialogManager, new AutocompleteSchemeClassifierImpl(),
                /** vrHandler= */ null,
                /** isSiteSettingsAvailable= */
                isHttpOrHttps(webContents.getVisibleUrl()),
                /** cookieControlsShown= */
                CookieControlsBridge.isCookieControlsEnabled(profile));
        mContext = context;
        mWebContents = webContents;
        mProfile = profile;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void showSiteSettings(String url) {
        Intent intent = SiteSettingsIntentHelper.createIntentForSingleWebsite(
                mContext, mProfile.getName(), mProfile.isIncognito(), url);

        // Disabling StrictMode to avoid violations (https://crbug.com/819410).
        launchIntent(intent);
    }

    @Override
    public void showCookieSettings() {
        String category = SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.COOKIES);
        String title = mContext.getResources().getString(
                ContentSettingsResources.getTitle(ContentSettingsType.COOKIES));
        Intent intent = SiteSettingsIntentHelper.createIntentForSingleCategory(
                mContext, mProfile.getName(), mProfile.isIncognito(), category, title);
        launchIntent(intent);
    }

    private void launchIntent(Intent intent) {
        // Disabling StrictMode to avoid violations (https://crbug.com/819410).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mContext.startActivity(intent);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public CookieControlsBridge createCookieControlsBridge(CookieControlsObserver observer) {
        return new CookieControlsBridge(observer, mWebContents, null);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public BrowserContextHandle getBrowserContext() {
        return mProfile;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public SiteSettingsClient getSiteSettingsClient() {
        return new WebLayerSiteSettingsClient(getBrowserContext());
    }

    @Override
    public void getFavicon(String url, Callback<Drawable> callback) {
        mProfile.getCachedFaviconForPageUri(
                url, ObjectWrapper.wrap((ValueCallback<Bitmap>) (bitmap) -> {
                    if (bitmap != null) {
                        callback.onResult(new BitmapDrawable(mContext.getResources(), bitmap));
                    } else {
                        callback.onResult(null);
                    }
                }));
    }

    private static boolean isHttpOrHttps(GURL url) {
        String scheme = url.getScheme();
        return UrlConstants.HTTP_SCHEME.equals(scheme) || UrlConstants.HTTPS_SCHEME.equals(scheme);
    }
}
