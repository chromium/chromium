// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.settings;

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

import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.FragmentHostingRemoteFragmentImpl;
import org.chromium.weblayer_private.ProfileImpl;
import org.chromium.weblayer_private.ProfileManager;
import org.chromium.weblayer_private.R;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ISettingsFragment;
import org.chromium.weblayer_private.interfaces.ISiteSettingsFragment;
import org.chromium.weblayer_private.interfaces.SettingsFragmentArgs;
import org.chromium.weblayer_private.interfaces.SettingsIntentHelper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * WebLayer's implementation of the client library's SettingsFragment.
 *
 * This class creates an instance of the Fragment given in its FRAGMENT_NAME argument.
 */
public class SettingsFragmentImpl extends FragmentHostingRemoteFragmentImpl {
    private static final String FRAGMENT_TAG = "settings_fragment";

    private final ProfileImpl mProfile;
    private final Class<? extends PreferenceFragmentCompat> mFragmentClass;
    private final Bundle mFragmentArguments;

    private static class SettingsContext
            extends FragmentHostingRemoteFragmentImpl.RemoteFragmentContext
            implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {
        private final Context mEmbedderContext;
        private final SettingsFragmentImpl mFragmentImpl;

        public SettingsContext(SettingsFragmentImpl fragmentImpl, Context embedderContext) {
            super(new ContextThemeWrapper(ClassLoaderContextWrapperFactory.get(embedderContext),
                    R.style.Theme_WebLayer_Settings));
            getTheme().applyStyle(R.style.ColorOverlay_WebLayer, /*force=*/true);
            mEmbedderContext = embedderContext;
            mFragmentImpl = fragmentImpl;
        }

        @Override
        public boolean onPreferenceStartFragment(
                PreferenceFragmentCompat caller, Preference preference) {
            // WebLayer's SettingsActivity structures its arguments differently than the
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
                intent = SettingsIntentHelper.createIntentForSiteSettingsCategoryList(
                        mEmbedderContext, profile.getName(), profile.isIncognito());
            } else if (newFragmentClassName.equals(SingleCategorySettings.class.getName())) {
                intent = SettingsIntentHelper.createIntentForSiteSettingsSingleCategory(
                        mEmbedderContext, profile.getName(), profile.isIncognito(),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_CATEGORY),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_TITLE));
            } else if (newFragmentClassName.equals(AllSiteSettings.class.getName())) {
                intent = SettingsIntentHelper.createIntentForSiteSettingsAllSites(mEmbedderContext,
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
                intent = SettingsIntentHelper.createIntentForSiteSettingsSingleWebsite(
                        mEmbedderContext, profile.getName(), profile.isIncognito(),
                        address.getOrigin());
            } else if (newFragmentClassName.equals(AccessibilitySettings.class.getName())) {
                intent = SettingsIntentHelper.createIntentForAccessibilitySettings(
                        mEmbedderContext, profile.getName(), profile.isIncognito());
            } else {
                throw new IllegalArgumentException("Unsupported Fragment: " + newFragmentClassName);
            }
            mFragmentImpl.getActivity().startActivity(intent);
            return true;
        }
    }

    public SettingsFragmentImpl(ProfileManager profileManager,
            IRemoteFragmentClient remoteFragmentClient, Bundle intentExtras) {
        super(remoteFragmentClient);
        String profileName = intentExtras.getString(SettingsFragmentArgs.PROFILE_NAME);
        boolean isIncognito = intentExtras.getBoolean(
                SettingsFragmentArgs.IS_INCOGNITO_PROFILE, /*defaultValue=*/profileName.equals(""));
        mProfile = profileManager.getProfile(profileName, isIncognito);

        // Convert the WebLayer ABI's Site Settings arguments into the format the Site Settings
        // implementation fragments expect.
        Bundle fragmentArgs = intentExtras.getBundle(SettingsFragmentArgs.FRAGMENT_ARGUMENTS);
        switch (intentExtras.getString(SettingsFragmentArgs.FRAGMENT_NAME)) {
            case SettingsFragmentArgs.ALL_SITES:
                mFragmentClass = AllSiteSettings.class;
                mFragmentArguments = new Bundle();
                mFragmentArguments.putString(AllSiteSettings.EXTRA_TITLE,
                        fragmentArgs.getString(SettingsFragmentArgs.ALL_SITES_TITLE));
                mFragmentArguments.putString(AllSiteSettings.EXTRA_CATEGORY,
                        fragmentArgs.getString(SettingsFragmentArgs.ALL_SITES_TYPE));
                break;
            case SettingsFragmentArgs.CATEGORY_LIST:
                mFragmentClass = SiteSettings.class;
                mFragmentArguments = null;
                break;
            case SettingsFragmentArgs.SINGLE_CATEGORY:
                mFragmentClass = SingleCategorySettings.class;
                mFragmentArguments = new Bundle();
                mFragmentArguments.putString(SingleCategorySettings.EXTRA_TITLE,
                        fragmentArgs.getString(SettingsFragmentArgs.SINGLE_CATEGORY_TITLE));
                mFragmentArguments.putString(SingleCategorySettings.EXTRA_CATEGORY,
                        fragmentArgs.getString(SettingsFragmentArgs.SINGLE_CATEGORY_TYPE));
                break;
            case SettingsFragmentArgs.SINGLE_WEBSITE:
                mFragmentClass = SingleWebsiteSettings.class;
                mFragmentArguments = SingleWebsiteSettings.createFragmentArgsForSite(
                        fragmentArgs.getString(SettingsFragmentArgs.SINGLE_WEBSITE_URL));
                break;
            case SettingsFragmentArgs.ACCESSIBILITY:
                mFragmentClass = AccessibilitySettings.class;
                mFragmentArguments = null;
                break;
            default:
                throw new IllegalArgumentException("Unknown Site Settings Fragment");
        }
    }

    @Override
    protected FragmentHostingRemoteFragmentImpl.RemoteFragmentContext createRemoteFragmentContext(
            Context embedderContext) {
        return new SettingsContext(this, embedderContext);
    }

    @Override
    public View onCreateView(ViewGroup container, Bundle savedInstanceState) {
        LayoutInflater inflater = (LayoutInflater) getWebLayerContext().getSystemService(
                Context.LAYOUT_INFLATER_SERVICE);
        if (getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG) == null) {
            try {
                PreferenceFragmentCompat settingsFragment = mFragmentClass.newInstance();
                settingsFragment.setArguments(mFragmentArguments);
                if (settingsFragment instanceof SiteSettingsPreferenceFragment) {
                    ((SiteSettingsPreferenceFragment) settingsFragment)
                            .setSiteSettingsDelegate(new WebLayerSiteSettingsDelegate(mProfile));
                } else if (settingsFragment instanceof AccessibilitySettings) {
                    ((AccessibilitySettings) settingsFragment)
                            .setDelegate(new WebLayerAccessibilitySettingsDelegate(mProfile));
                }
                getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.settings_container, settingsFragment, FRAGMENT_TAG)
                        .commitNow();
            } catch (IllegalAccessException | InstantiationException e) {
                throw new RuntimeException("Failed to create Settings Fragment", e);
            }
        }

        View root = inflater.inflate(R.layout.settings_layout, container, /*attachToRoot=*/false);
        root.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {
                // Add the shadow scroll listener here once the View is attached to the Window.
                PreferenceFragmentCompat preferenceFragment =
                        (PreferenceFragmentCompat) getSupportFragmentManager().findFragmentByTag(
                                FRAGMENT_TAG);
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

    public ISettingsFragment asISettingsFragment() {
        return new ISettingsFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return SettingsFragmentImpl.this;
            }
        };
    }

    public ISiteSettingsFragment asISiteSettingsFragment() {
        return new ISiteSettingsFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return SettingsFragmentImpl.this;
            }
        };
    }

    private ProfileImpl getProfile() {
        return mProfile;
    }
}
