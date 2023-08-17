// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.RemoteException;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
import org.chromium.weblayer_private.interfaces.DarkModeStrategy;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IMediaRouteDialogFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.media.MediaRouteDialogFragmentImpl;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Implementation of {@link IBrowser}.
 */
@JNINamespace("weblayer")
public class BrowserImpl extends IBrowser.Stub {
    private final ObserverList<VisibleSecurityStateObserver> mVisibleSecurityStateObservers =
            new ObserverList<VisibleSecurityStateObserver>();

    // Number of instances that have not been destroyed.
    private static int sInstanceCount;

    private long mNativeBrowser;
    private final ProfileImpl mProfile;
    private final boolean mIsExternalIntentsEnabled;
    private Context mServiceContext;

    private @Nullable List<Origin> mAllowedOrigins;

    private IBrowserClient mClient;
    private boolean mInDestroy;

    // Cache the value instead of querying system every time.
    private Boolean mPasswordEchoEnabled;
    private Boolean mDarkThemeEnabled;
    @DarkModeStrategy
    private int mDarkModeStrategy = DarkModeStrategy.WEB_THEME_DARKENING_ONLY;
    private Float mFontScale;

    // Created in the constructor from saved state.
    private FullPersistenceInfo mFullPersistenceInfo;

    private BrowserFragmentImpl mBrowserFragmentImpl;

    // This persistence state is saved to disk, and loaded async.
    private static final class FullPersistenceInfo {
        String mPersistenceId;
    };
    /**
     * Allows observing of visible security state of the active tab.
     */
    public static interface VisibleSecurityStateObserver {
        public void onVisibleSecurityStateOfActiveTabChanged();
    }
    public void addVisibleSecurityStateObserver(VisibleSecurityStateObserver observer) {
        mVisibleSecurityStateObservers.addObserver(observer);
    }
    public void removeVisibleSecurityStateObserver(VisibleSecurityStateObserver observer) {
        mVisibleSecurityStateObservers.removeObserver(observer);
    }

    public BrowserImpl(Context serviceContext, ProfileManager profileManager, Bundle fragmentArgs) {
        ++sInstanceCount;
        mServiceContext = serviceContext;

        String persistenceId = fragmentArgs.getString(BrowserFragmentArgs.PERSISTENCE_ID);
        String name = fragmentArgs.getString(BrowserFragmentArgs.PROFILE_NAME);

        boolean isIncognito;
        if (fragmentArgs.containsKey(BrowserFragmentArgs.IS_INCOGNITO)) {
            isIncognito = fragmentArgs.getBoolean(BrowserFragmentArgs.IS_INCOGNITO, false);
        } else {
            isIncognito = "".equals(name);
        }
        mProfile = profileManager.getProfile(name, isIncognito);

        mProfile.checkNotDestroyed(); // TODO(swestphal): or mProfile != null

        mIsExternalIntentsEnabled =
                fragmentArgs.getBoolean(BrowserFragmentArgs.IS_EXTERNAL_INTENTS_ENABLED);

        List<String> allowedOriginStrings =
                fragmentArgs.getStringArrayList(BrowserFragmentArgs.ALLOWED_ORIGINS);
        if (allowedOriginStrings != null) {
            mAllowedOrigins = new ArrayList<Origin>();

            for (String allowedOriginString : allowedOriginStrings) {
                Origin allowedOrigin = Origin.create(allowedOriginString);
                if (allowedOrigin != null) {
                    mAllowedOrigins.add(allowedOrigin);
                }
            }
        }

        if (!isIncognito && !TextUtils.isEmpty(persistenceId)) {
            mFullPersistenceInfo = new FullPersistenceInfo();
            mFullPersistenceInfo.mPersistenceId = persistenceId;
        }

        mNativeBrowser = BrowserImplJni.get().createBrowser(
                mProfile.getNativeProfile(), serviceContext.getPackageName(), this);
        mPasswordEchoEnabled = null;
    }

    @Override
    public IRemoteFragment createBrowserFragmentImpl() {
        StrictModeWorkaround.apply();
        mBrowserFragmentImpl = new BrowserFragmentImpl(this, mServiceContext);
        return mBrowserFragmentImpl;
    }

    @Override
    public IMediaRouteDialogFragment createMediaRouteDialogFragmentImpl() {
        StrictModeWorkaround.apply();
        MediaRouteDialogFragmentImpl fragment = new MediaRouteDialogFragmentImpl(mServiceContext);
        return fragment.asIMediaRouteDialogFragment();
    }

    public Context getContext() {
        return mBrowserFragmentImpl.getWebLayerContext();
    }

    @Override
    public TabImpl createTab() {
        StrictModeWorkaround.apply();
        TabImpl tab = new TabImpl(this, mProfile, mBrowserFragmentImpl.getWindowAndroid());
        // This needs |alwaysAdd| set to true as the Tab is created with the Browser already set to
        // this.
        addTab(tab, /* alwaysAdd */ true);
        return tab;
    }

    @Override
    @NonNull
    public ProfileImpl getProfile() {
        StrictModeWorkaround.apply();
        return mProfile;
    }

    @Override
    public void addTab(ITab iTab) {
        StrictModeWorkaround.apply();
        addTab((TabImpl) iTab, /* alwaysAdd */ false);
    }

    private void addTab(TabImpl tab, boolean alwaysAdd) {
        if (!alwaysAdd && tab.getBrowser() == this) return;
        BrowserImplJni.get().addTab(mNativeBrowser, tab.getNativeTab());
    }

    @CalledByNative
    private void createJavaTabForNativeTab(long nativeTab) {
        new TabImpl(this, mProfile, mBrowserFragmentImpl.getWindowAndroid(), nativeTab);
    }

    void checkPreferences() {
        boolean changed = false;
        if (mPasswordEchoEnabled != null) {
            boolean oldEnabled = mPasswordEchoEnabled;
            mPasswordEchoEnabled = null;
            boolean newEnabled = getPasswordEchoEnabled();
            changed = changed || oldEnabled != newEnabled;
        }
        if (mDarkThemeEnabled != null) {
            boolean oldEnabled = mDarkThemeEnabled;
            mDarkThemeEnabled = null;
            boolean newEnabled = getDarkThemeEnabled();
            changed = changed || oldEnabled != newEnabled;
        }
        if (changed) {
            BrowserImplJni.get().webPreferencesChanged(mNativeBrowser);
        }
    }

    @CalledByNative
    private boolean getPasswordEchoEnabled() {
        Context context = getContext();
        if (context == null) return false;
        if (mPasswordEchoEnabled == null) {
            mPasswordEchoEnabled = Settings.System.getInt(context.getContentResolver(),
                                           Settings.System.TEXT_SHOW_PASSWORD, 1)
                    == 1;
        }
        return mPasswordEchoEnabled;
    }

    @CalledByNative
    boolean getDarkThemeEnabled() {
        if (mServiceContext == null) return false;
        if (mDarkThemeEnabled == null) {
            if (mServiceContext == null) return false;
            int uiMode = mServiceContext.getApplicationContext()
                                 .getResources()
                                 .getConfiguration()
                                 .uiMode;
            mDarkThemeEnabled =
                    (uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        }
        return mDarkThemeEnabled;
    }

    @CalledByNative
    private void onTabAdded(TabImpl tab) throws RemoteException {
        tab.attachToBrowser(this);
        if (mClient != null) mClient.onTabAdded(tab);
    }

    @CalledByNative
    private void onActiveTabChanged(TabImpl tab) throws RemoteException {
        mBrowserFragmentImpl.setActiveTab(tab);
        if (!mInDestroy && mClient != null) {
            mClient.onActiveTabChanged(tab != null ? tab.getId() : 0);
        }
    }

    @CalledByNative
    private void onTabRemoved(TabImpl tab) throws RemoteException {
        if (mInDestroy) return;
        if (mClient != null) mClient.onTabRemoved(tab.getId());
        // This doesn't reset state on TabImpl as |browser| is either about to be
        // destroyed, or switching to a different fragment.
    }

    @CalledByNative
    private void onVisibleSecurityStateOfActiveTabChanged() {
        for (VisibleSecurityStateObserver observer : mVisibleSecurityStateObservers) {
            observer.onVisibleSecurityStateOfActiveTabChanged();
        }
    }

    @CalledByNative
    private boolean compositorHasSurface() {
        return mBrowserFragmentImpl.compositorHasSurface();
    }

    @Override
    public boolean setActiveTab(ITab iTab) {
        StrictModeWorkaround.apply();
        TabImpl tab = (TabImpl) iTab;
        if (tab != null && tab.getBrowser() != this) return false;
        BrowserImplJni.get().setActiveTab(mNativeBrowser, tab != null ? tab.getNativeTab() : 0);
        mBrowserFragmentImpl.setActiveTab(tab);
        return true;
    }

    public @Nullable TabImpl getActiveTab() {
        return BrowserImplJni.get().getActiveTab(mNativeBrowser);
    }

    @Override
    public List<TabImpl> getTabs() {
        StrictModeWorkaround.apply();
        return Arrays.asList(BrowserImplJni.get().getTabs(mNativeBrowser));
    }

    @Override
    public int getActiveTabId() {
        StrictModeWorkaround.apply();
        return getActiveTab() != null ? getActiveTab().getId() : 0;
    }

    @Override
    public int[] getTabIds() {
        StrictModeWorkaround.apply();
        List<TabImpl> tabs = getTabs();
        int[] ids = new int[tabs.size()];
        for(int i = 0; i < tabs.size(); i++) {
            ids[i] = tabs.get(i).getId();
        }
        return ids;
    }

    boolean isExternalIntentsEnabled() {
        return mIsExternalIntentsEnabled;
    }

    boolean isUrlAllowed(String url) {
        // Defaults to all origins being allowed if a developer list is not provided.
        if (mAllowedOrigins == null) {
            return true;
        }

        return mAllowedOrigins.contains(Origin.create(url));
    }

    public boolean isWindowOnSmallDevice() {
        WindowAndroid windowAndroid = mBrowserFragmentImpl.getWindowAndroid();
        assert windowAndroid != null;
        return !DeviceFormFactor.isWindowOnTablet(windowAndroid);
    }

    @Override
    public void setClient(IBrowserClient client) {
        // This function is called from the client once everything has been setup (meaning all the
        // client classes have been created and AIDL interfaces established in both directions).
        // This function is called immediately after the constructor of BrowserImpl from the client.

        StrictModeWorkaround.apply();
        mClient = client;

        if (mFullPersistenceInfo != null) {
            FullPersistenceInfo persistenceInfo = mFullPersistenceInfo;
            mFullPersistenceInfo = null;
            BrowserImplJni.get().restoreStateIfNecessary(
                    mNativeBrowser, persistenceInfo.mPersistenceId);
        } else {
            boolean setActiveResult = setActiveTab(createTab());
            assert setActiveResult;
            try {
                onTabInitializationCompleted();
            } catch (RemoteException e) {
            }
        }
    }

    @Override
    public void destroyTab(ITab iTab) {
        StrictModeWorkaround.apply();
        TabImpl tab = (TabImpl) iTab;
        if (tab.getBrowser() != this) return;
        destroyTabImpl((TabImpl) iTab);
    }

    @CalledByNative
    private void destroyTabImpl(TabImpl tab) {
        tab.destroy();
    }

    @Override
    public void setDarkModeStrategy(@DarkModeStrategy int strategy) {
        StrictModeWorkaround.apply();
        if (mDarkModeStrategy == strategy) {
            return;
        }
        mDarkModeStrategy = strategy;
        BrowserImplJni.get().webPreferencesChanged(mNativeBrowser);
    }

    @CalledByNative
    int getDarkModeStrategy() {
        return mDarkModeStrategy;
    }

    @Override
    public boolean isRestoringPreviousState() {
        StrictModeWorkaround.apply();
        return BrowserImplJni.get().isRestoringPreviousState(mNativeBrowser);
    }

    @CalledByNative
    private void onRestoreCompleted() throws RemoteException {
        mClient.onTabInitializationCompleted();
    }

    private void onTabInitializationCompleted() throws RemoteException {
        mClient.onTabInitializationCompleted();
    }

    @Override
    public void shutdown() {
        StrictModeWorkaround.apply();
        mInDestroy = true;

        BrowserImplJni.get().prepareForShutdown(mNativeBrowser);

        for (Object tab : getTabs()) {
            destroyTabImpl((TabImpl) tab);
        }
        mBrowserFragmentImpl.shutdown();
        BrowserImplJni.get().deleteBrowser(mNativeBrowser);

        mVisibleSecurityStateObservers.clear();

        --sInstanceCount;
    }

    void updateAllTabsViewAttachedState() {
        for (Object tab : getTabs()) {
            ((TabImpl) tab).updateViewAttachedStateFromBrowser();
        }
    }

    void updateAllTabs() {
        for (Object tab : getTabs()) {
            ((TabImpl) tab).updateFromBrowser();
        }
    }

    long getNativeBrowser() {
        return mNativeBrowser;
    }

    public BrowserFragmentImpl getBrowserFragment() {
        return mBrowserFragmentImpl;
    }

    @NativeMethods
    interface Natives {
        long createBrowser(long profile, String packageName, BrowserImpl caller);
        void deleteBrowser(long browser);
        void addTab(long nativeBrowserImpl, long nativeTab);
        TabImpl[] getTabs(long nativeBrowserImpl);
        void setActiveTab(long nativeBrowserImpl, long nativeTab);
        TabImpl getActiveTab(long nativeBrowserImpl);
        void prepareForShutdown(long nativeBrowserImpl);
        void restoreStateIfNecessary(long nativeBrowserImpl, String persistenceId);
        void webPreferencesChanged(long nativeBrowserImpl);
        boolean isRestoringPreviousState(long nativeBrowserImpl);
    }
}
