// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ISiteSettingsFragment;
import org.chromium.weblayer_private.interfaces.SiteSettingsFragmentArgs;
import org.chromium.weblayer_private.interfaces.SiteSettingsIntentHelper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * WebLayer's implementation of the client library's SiteSettingsFragment.
 *
 * This class creates an instance of the Fragment given in its FRAGMENT_NAME argument.
 */
public class SiteSettingsFragmentImpl extends FragmentHostingRemoteFragmentImpl {
    private static final String FRAGMENT_TAG = "site_settings_fragment";

    private final ProfileImpl mProfile;
    private final Class<? extends SiteSettingsPreferenceFragment> mFragmentClass;
    private final Bundle mFragmentArguments;

    private static class SiteSettingsContext
            extends FragmentHostingRemoteFragmentImpl.RemoteFragmentContext
            implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {
        private final Context mEmbedderContext;
        private final SiteSettingsFragmentImpl mFragmentImpl;

        public SiteSettingsContext(SiteSettingsFragmentImpl fragmentImpl, Context embedderContext) {
            super(new ContextThemeWrapper(ClassLoaderContextWrapperFactory.get(embedderContext),
                    R.style.Theme_WebLayer_SiteSettings));
            mEmbedderContext = embedderContext;
            mFragmentImpl = fragmentImpl;
        }

        @Override
        public boolean onPreferenceStartFragment(
                PreferenceFragmentCompat caller, Preference preference) {
            // WebLayer's SiteSettingsActivity structures its arguments differently than the
            // implementation Fragments do. This is to avoid hardcoding implementation class names
            // in //components in the API, and because the Fragments in //components rely on
            // passing serialized Objects to each other, which aren't passable to the embedder
            // because they live in different ClassLoaders. This block of code translates the
            // Fragment arguments provided by the implementation Fragments to what the WebLayer API
            // expects, and then tells the client-side Activity to start the new Site Settings page.
            Intent intent;
            String newFragmentClassName = preference.getFragment();
            Bundle newFragmentArgs = preference.getExtras();
            ProfileImpl profile = mFragmentImpl.getProfile();
            if (newFragmentClassName.equals(SiteSettings.class.getName())) {
                intent = SiteSettingsIntentHelper.createIntentForCategoryList(
                        mEmbedderContext, profile.getName(), profile.isIncognito());
            } else if (newFragmentClassName.equals(SingleCategorySettings.class.getName())) {
                intent = SiteSettingsIntentHelper.createIntentForSingleCategory(mEmbedderContext,
                        profile.getName(), profile.isIncognito(),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_CATEGORY),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_TITLE));
            } else if (newFragmentClassName.equals(AllSiteSettings.class.getName())) {
                intent = SiteSettingsIntentHelper.createIntentForAllSites(mEmbedderContext,
                        profile.getName(), profile.isIncognito(),
                        newFragmentArgs.getString(AllSiteSettings.EXTRA_CATEGORY),
                        newFragmentArgs.getString(AllSiteSettings.EXTRA_TITLE));
            } else if (newFragmentClassName.equals(SingleWebsiteSettings.class.getName())) {
                WebsiteAddress address;
                if (newFragmentArgs.containsKey(SingleWebsiteSettings.EXTRA_SITE)) {
                    Website website = (Website) newFragmentArgs.getSerializable(
                            SingleWebsiteSettings.EXTRA_SITE);
                    address = website.getAddress();
                } else if (newFragmentArgs.containsKey(SingleWebsiteSettings.EXTRA_SITE_ADDRESS)) {
                    address = (WebsiteAddress) newFragmentArgs.getSerializable(
                            SingleWebsiteSettings.EXTRA_SITE_ADDRESS);
                } else {
                    throw new IllegalArgumentException("No website provided");
                }
                intent = SiteSettingsIntentHelper.createIntentForSingleWebsite(mEmbedderContext,
                        profile.getName(), profile.isIncognito(), address.getOrigin());
            } else {
                throw new IllegalArgumentException("Unsupported Fragment: " + newFragmentClassName);
            }
            mFragmentImpl.getActivity().startActivity(intent);
            return true;
        }
    }

    public SiteSettingsFragmentImpl(ProfileManager profileManager,
            IRemoteFragmentClient remoteFragmentClient, Bundle intentExtras) {
        super(remoteFragmentClient);
        String profileName = intentExtras.getString(SiteSettingsFragmentArgs.PROFILE_NAME);
        boolean isIncognito;
        if (intentExtras.containsKey(SiteSettingsFragmentArgs.IS_INCOGNITO_PROFILE)) {
            isIncognito =
                    intentExtras.getBoolean(SiteSettingsFragmentArgs.IS_INCOGNITO_PROFILE, false);
        } else {
            isIncognito = "".equals(profileName);
        }
        mProfile = profileManager.getProfile(profileName, isIncognito);

        // Convert the WebLayer ABI's Site Settings arguments into the format the Site Settings
        // implementation fragments expect.
        Bundle fragmentArgs = intentExtras.getBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS);
        switch (intentExtras.getString(SiteSettingsFragmentArgs.FRAGMENT_NAME)) {
            case SiteSettingsFragmentArgs.ALL_SITES:
                mFragmentClass = AllSiteSettings.class;
                mFragmentArguments = new Bundle();
                mFragmentArguments.putString(AllSiteSettings.EXTRA_TITLE,
                        fragmentArgs.getString(SiteSettingsFragmentArgs.ALL_SITES_TITLE));
                mFragmentArguments.putString(AllSiteSettings.EXTRA_CATEGORY,
                        fragmentArgs.getString(SiteSettingsFragmentArgs.ALL_SITES_TYPE));
                break;
            case SiteSettingsFragmentArgs.CATEGORY_LIST:
                mFragmentClass = SiteSettings.class;
                mFragmentArguments = null;
                break;
            case SiteSettingsFragmentArgs.SINGLE_CATEGORY:
                mFragmentClass = SingleCategorySettings.class;
                mFragmentArguments = new Bundle();
                mFragmentArguments.putString(SingleCategorySettings.EXTRA_TITLE,
                        fragmentArgs.getString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TITLE));
                mFragmentArguments.putString(SingleCategorySettings.EXTRA_CATEGORY,
                        fragmentArgs.getString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TYPE));
                break;
            case SiteSettingsFragmentArgs.SINGLE_WEBSITE:
                mFragmentClass = SingleWebsiteSettings.class;
                mFragmentArguments = SingleWebsiteSettings.createFragmentArgsForSite(
                        fragmentArgs.getString(SiteSettingsFragmentArgs.SINGLE_WEBSITE_URL));
                break;
            default:
                throw new IllegalArgumentException("Unknown Site Settings Fragment");
        }
    }

    @Override
    protected FragmentHostingRemoteFragmentImpl.RemoteFragmentContext createRemoteFragmentContext(
            Context embedderContext) {
        return new SiteSettingsContext(this, embedderContext);
    }

    @Override
    public View onCreateView(ViewGroup container, Bundle savedInstanceState) {
        LayoutInflater inflater = (LayoutInflater) getWebLayerContext().getSystemService(
                Context.LAYOUT_INFLATER_SERVICE);
        View root =
                inflater.inflate(R.layout.site_settings_layout, container, /*attachToRoot=*/false);
        if (getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG) == null) {
            try {
                SiteSettingsPreferenceFragment siteSettingsFragment = mFragmentClass.newInstance();
                siteSettingsFragment.setArguments(mFragmentArguments);
                siteSettingsFragment.setSiteSettingsClient(
                        new WebLayerSiteSettingsClient(mProfile));
                getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.site_settings_container, siteSettingsFragment, FRAGMENT_TAG)
                        .commitNow();
            } catch (IllegalAccessException | InstantiationException e) {
                throw new RuntimeException("Failed to create Site Settings Fragment", e);
            }
        }

        root.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {
                // Add the shadow scroll listener here once the View is attached to the Window.
                SiteSettingsPreferenceFragment preferenceFragment =
                        (SiteSettingsPreferenceFragment) getSupportFragmentManager()
                                .findFragmentByTag(FRAGMENT_TAG);
                ViewGroup listView = preferenceFragment.getListView();
                listView.getViewTreeObserver().addOnScrollChangedListener(
                        SettingsUtils.getShowShadowOnScrollListener(
                                listView, view.findViewById(R.id.shadow)));
            }

            @Override
            public void onViewDetachedFromWindow(View v) {}
        });
        return root;
    }

    public ISiteSettingsFragment asISiteSettingsFragment() {
        return new ISiteSettingsFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return SiteSettingsFragmentImpl.this;
            }
        };
    }

    private ProfileImpl getProfile() {
        return mProfile;
    }
}
