// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.Window;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentController;
import androidx.fragment.app.FragmentHostCallback;
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

import java.lang.reflect.Constructor;

/**
 * WebLayer's implementation of the client library's SiteSettingsFragment.
 *
 * This class creates an instance of the Fragment given in its FRAGMENT_NAME argument, and forwards
 * all incoming lifecycle events from SiteSettingsFragment to it. Because Fragments created in
 * WebLayer use the AndroidX library from WebLayer's ClassLoader, we can't attach the Fragment
 * created here directly to the embedder's Fragment tree, and have to create a local
 * FragmentController to manage it.
 */
public class SiteSettingsFragmentImpl extends RemoteFragmentImpl {
    private static final String FRAGMENT_TAG = "site_settings_fragment";

    private final ProfileImpl mProfile;
    private final Class<? extends SiteSettingsPreferenceFragment> mFragmentClass;
    private final Bundle mFragmentArguments;

    // The embedder's original context object.
    private Context mEmbedderContext;

    // The WebLayer-wrapped context object. This context gets assets and resources from WebLayer,
    // not from the embedder. Use this for the most part, especially to resolve WebLayer-specific
    // resource IDs.
    private Context mContext;

    private boolean mStarted;
    private FragmentController mFragmentController;

    /**
     * A fake FragmentActivity needed to make the Fragment system happy.
     *
     * PreferenceFragmentCompat calls Fragment.getActivity, which casts the Activity given to the
     * FragmentHostCallback to a FragmentActivity. Because of the AndroidX ClassLoader issue
     * mentioned aove, this cast will fail if we use the embedder's Activity because the
     * FragmentActivity it derives from lives in another ClassLoader.  This class exists to provide
     * a FragmentActivity for the Site Settings Fragments to run in, and forwards necessary methods
     * to the remote Activity.
     */
    private static class PassthroughFragmentActivity extends FragmentActivity
            implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {
        private static final Class<?>[] VIEW_CONSTRUCTOR_ARGS =
                new Class[] {Context.class, AttributeSet.class};

        private final SiteSettingsFragmentImpl mFragmentImpl;

        private PassthroughFragmentActivity(SiteSettingsFragmentImpl fragmentImpl) {
            mFragmentImpl = fragmentImpl;
            attachBaseContext(mFragmentImpl.getWebLayerContext());
            // Register ourselves as a the LayoutInflater factory so we can handle loading Views.
            // See onCreateView for information about why this is needed.
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
                getLayoutInflater().setFactory2(this);
            }
            // This class doesn't extend AppCompatActivity, so some appcompat functionality doesn't
            // get initialized, which leads to some appcompat widgets (like switches) rendering
            // incorrectly. There are some resource issues with having this class extend
            // AppCompatActivity, but until we sort those out, creating an AppCompatDelegate will
            // perform the necessary initialization.
            AppCompatDelegate.create(this, null);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                return getLayoutInflater();
            }
            return getEmbedderActivity().getSystemService(name);
        }

        @Override
        public LayoutInflater getLayoutInflater() {
            return (LayoutInflater) getBaseContext().getSystemService(
                    Context.LAYOUT_INFLATER_SERVICE);
        }

        // This method is needed to work around a LayoutInflater bug in Android <N.  Before
        // LayoutInflater creates an instance of a View, it needs to look up the class by name to
        // get a reference to its Constructor. As an optimization, it caches this name to
        // Constructor mapping. This cache causes issues if a class gets loaded multiple times with
        // different ClassLoaders. In Site Settings, some AndroidX Views get loaded early on with
        // the embedding app's ClassLoader, so the Constructor from that ClassLoader's version of
        // the class gets cached. When the WebLayer implementation later tries to inflate the same
        // class, it instantiates a version from the wrong ClassLoader, which leads to a
        // ClassCastException when casting that View to its original class. This was fixed in
        // Android N, but to work around it on L & M, we inflate the Views manually here, which
        // bypasses LayoutInflater's cache.
        @Override
        public View onCreateView(View parent, String name, Context context, AttributeSet attrs) {
            // If the class doesn't have a '.' in its name, it's probably a built-in Android View,
            // which are often referenced by just their class names with no package prefix. For
            // these classes we can return null to fall back to LayoutInflater's default behavior.
            if (name.indexOf('.') == -1) {
                return null;
            }

            Class<? extends View> clazz = null;
            try {
                clazz = context.getClassLoader().loadClass(name).asSubclass(View.class);
                LayoutInflater inflater = getLayoutInflater();
                if (inflater.getFilter() != null && !inflater.getFilter().onLoadClass(clazz)) {
                    throw new InflateException(attrs.getPositionDescription()
                            + ": Class not allowed to be inflated " + name);
                }

                Constructor<? extends View> constructor =
                        clazz.getConstructor(VIEW_CONSTRUCTOR_ARGS);
                constructor.setAccessible(true);
                View view = constructor.newInstance(new Object[] {context, attrs});
                if (view instanceof ViewStub) {
                    // Use the same Context when inflating ViewStub later.
                    ViewStub viewStub = (ViewStub) view;
                    viewStub.setLayoutInflater(inflater.cloneInContext(context));
                }
                return view;
            } catch (Exception e) {
                InflateException ie = new InflateException(attrs.getPositionDescription()
                        + ": Error inflating class "
                        + (clazz == null ? "<unknown>" : clazz.getName()));
                ie.initCause(e);
                throw ie;
            }
        }

        @Override
        public Window getWindow() {
            return getEmbedderActivity().getWindow();
        }

        @Override
        public Context getApplicationContext() {
            return getEmbedderActivity().getApplicationContext();
        }

        @Override
        public void startActivity(Intent intent) {
            getEmbedderActivity().startActivity(intent);
        }

        @Override
        public void setTitle(int titleId) {
            getEmbedderActivity().setTitle(mFragmentImpl.getWebLayerContext().getString(titleId));
        }

        @Override
        public void setTitle(CharSequence title) {
            getEmbedderActivity().setTitle(title);
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
                        mFragmentImpl.getEmbedderContext(), profile.getName(),
                        profile.isIncognito());
            } else if (newFragmentClassName.equals(SingleCategorySettings.class.getName())) {
                intent = SiteSettingsIntentHelper.createIntentForSingleCategory(
                        mFragmentImpl.getEmbedderContext(), profile.getName(),
                        profile.isIncognito(),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_CATEGORY),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_TITLE));
            } else if (newFragmentClassName.equals(AllSiteSettings.class.getName())) {
                intent = SiteSettingsIntentHelper.createIntentForAllSites(
                        mFragmentImpl.getEmbedderContext(), profile.getName(),
                        profile.isIncognito(),
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
                intent = SiteSettingsIntentHelper.createIntentForSingleWebsite(
                        mFragmentImpl.getEmbedderContext(), profile.getName(),
                        profile.isIncognito(), address.getOrigin());
            } else {
                throw new IllegalArgumentException("Unsupported Fragment: " + newFragmentClassName);
            }
            getEmbedderActivity().startActivity(intent);
            return true;
        }

        private Activity getEmbedderActivity() {
            return mFragmentImpl.getActivity();
        }
    }

    private static class SiteSettingsFragmentHostCallback extends FragmentHostCallback<Context> {
        private final SiteSettingsFragmentImpl mFragmentImpl;

        private SiteSettingsFragmentHostCallback(SiteSettingsFragmentImpl fragmentImpl) {
            super(new PassthroughFragmentActivity(fragmentImpl), new Handler(), 0);
            mFragmentImpl = fragmentImpl;
        }

        @Override
        public Context onGetHost() {
            return mFragmentImpl.getWebLayerContext();
        }

        @Override
        public LayoutInflater onGetLayoutInflater() {
            Context context = mFragmentImpl.getWebLayerContext();
            return ((LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                    .cloneInContext(context);
        }

        @Override
        public boolean onHasView() {
            return mFragmentImpl.getView() != null;
        }

        @Override
        public View onFindViewById(int id) {
            return onHasView() ? mFragmentImpl.getView().findViewById(id) : null;
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
    public void onAttach(Context context) {
        StrictModeWorkaround.apply();
        super.onAttach(context);

        mEmbedderContext = context;
        mContext = new ContextThemeWrapper(
                ClassLoaderContextWrapperFactory.get(context), R.style.Theme_WebLayer_SiteSettings);
        mFragmentController =
                FragmentController.createController(new SiteSettingsFragmentHostCallback(this));
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        StrictModeWorkaround.apply();
        mFragmentController.attachHost(null);

        super.onCreate(savedInstanceState);

        mFragmentController.dispatchCreate();
    }

    @Override
    public View onCreateView(ViewGroup container, Bundle savedInstanceState) {
        LayoutInflater inflater = (LayoutInflater) getWebLayerContext().getSystemService(
                Context.LAYOUT_INFLATER_SERVICE);
        View root =
                inflater.inflate(R.layout.site_settings_layout, container, /*attachToRoot=*/false);
        if (mFragmentController.getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG)
                == null) {
            try {
                SiteSettingsPreferenceFragment siteSettingsFragment = mFragmentClass.newInstance();
                siteSettingsFragment.setArguments(mFragmentArguments);
                siteSettingsFragment.setSiteSettingsClient(
                        new WebLayerSiteSettingsClient(mProfile));
                mFragmentController.getSupportFragmentManager()
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
                        (SiteSettingsPreferenceFragment) mFragmentController
                                .getSupportFragmentManager()
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

    @Override
    public void onDestroyView() {
        StrictModeWorkaround.apply();
        super.onDestroyView();
        mFragmentController.dispatchDestroyView();
    }

    @Override
    public void onDestroy() {
        StrictModeWorkaround.apply();
        super.onDestroy();
        mFragmentController.dispatchDestroy();
    }

    @Override
    public void onDetach() {
        StrictModeWorkaround.apply();
        super.onDetach();
        mContext = null;
    }

    @Override
    public void onStart() {
        super.onStart();

        if (!mStarted) {
            mStarted = true;
            mFragmentController.dispatchActivityCreated();
        }
        mFragmentController.noteStateNotSaved();
        mFragmentController.execPendingActions();
        mFragmentController.dispatchStart();
    }

    @Override
    public void onStop() {
        super.onStop();
        mFragmentController.dispatchStop();
    }

    @Override
    public void onResume() {
        super.onResume();
        mFragmentController.dispatchResume();
    }

    @Override
    public void onPause() {
        super.onPause();
        mFragmentController.dispatchPause();
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

    private Context getWebLayerContext() {
        return mContext;
    }

    private Context getEmbedderContext() {
        return mEmbedderContext;
    }

    private ProfileImpl getProfile() {
        return mProfile;
    }
}
