// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.RemoteException;
import android.provider.Settings;
import android.view.View;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.IUrlBarController;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.media.MediaRouteDialogFragmentImpl;

import java.util.Arrays;
import java.util.List;

/**
 * Implementation of {@link IBrowser}.
 */
@JNINamespace("weblayer")
public class BrowserImpl extends IBrowser.Stub implements View.OnAttachStateChangeListener {
    private final ObserverList<VisibleSecurityStateObserver> mVisibleSecurityStateObservers =
            new ObserverList<VisibleSecurityStateObserver>();

    // Key used to save the crypto key in instance state.
    public static final String SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY =
            "SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY";

    // Key used to save the minimal persistence state in instance state. Only used if a persistence
    // id was not specified.
    public static final String SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY =
            "SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY";

    // Number of instances that have not been destroyed.
    private static int sInstanceCount;

    private long mNativeBrowser;
    private final ProfileImpl mProfile;
    private Context mEmbedderActivityContext;
    private BrowserViewController mViewController;
    // Used to save UI state between destroyAttachmentState() and createAttachmentState() calls so
    // it can be preserved during device rotations or other events that cause the Fragment to be
    // recreated.
    private BrowserViewController.State mViewControllerState;
    private FragmentWindowAndroid mWindowAndroid;
    private IBrowserClient mClient;
    private LocaleChangedBroadcastReceiver mLocaleReceiver;
    private boolean mInDestroy;
    private final UrlBarControllerImpl mUrlBarController;
    private boolean mFragmentStarted;
    private boolean mFragmentResumed;
    private boolean mFragmentStoppedForConfigurationChange;
    // Cache the value instead of querying system every time.
    private Boolean mPasswordEchoEnabled;
    private Boolean mDarkThemeEnabled;
    private Float mFontScale;
    private boolean mViewAttachedToWindow;

    // Created in the constructor from saved state and used in setClient().
    private PersistenceInfo mPersistenceInfo;

    private static final class PersistenceInfo {
        String mPersistenceId;
        byte[] mCryptoKey;
        byte[] mMinimalPersistenceState;
    };

    /**
     * @param windowAndroid a window that was created by a {@link BrowserFragmentImpl}. It's not
     *         valid to call this method with other {@link WindowAndroid} instances. Typically this
     *         should be the {@link WindowAndroid} of a {@link WebContents}.
     * @return the associated BrowserImpl instance.
     */
    public static BrowserImpl fromWindowAndroid(WindowAndroid windowAndroid) {
        assert windowAndroid instanceof FragmentWindowAndroid;
        return ((FragmentWindowAndroid) windowAndroid).getBrowser();
    }

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

    public BrowserImpl(Context embedderAppContext, ProfileImpl profile, String persistenceId,
            Bundle savedInstanceState, FragmentWindowAndroid windowAndroid) {
        ++sInstanceCount;
        profile.checkNotDestroyed();
        mProfile = profile;

        mPersistenceInfo = new PersistenceInfo();
        mPersistenceInfo.mPersistenceId = persistenceId;
        mPersistenceInfo.mCryptoKey = savedInstanceState != null
                ? savedInstanceState.getByteArray(SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY)
                : null;
        mPersistenceInfo.mMinimalPersistenceState =
                (savedInstanceState != null && (persistenceId == null || persistenceId.isEmpty()))
                ? savedInstanceState.getByteArray(SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY)
                : null;

        windowAndroid.restoreInstanceState(savedInstanceState);

        createAttachmentState(embedderAppContext, windowAndroid);
        mNativeBrowser = BrowserImplJni.get().createBrowser(profile.getNativeProfile(), this);
        mUrlBarController = new UrlBarControllerImpl(this, mNativeBrowser);
    }

    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    public ContentView getViewAndroidDelegateContainerView() {
        if (mViewController == null) return null;
        return mViewController.getContentView();
    }

    public UrlBarControllerImpl getUrlBarControllerImpl() {
        return mUrlBarController;
    }

    // Called from constructor and onFragmentAttached() to configure state needed when attached.
    private void createAttachmentState(
            Context embedderAppContext, FragmentWindowAndroid windowAndroid) {
        assert mViewController == null;
        assert mWindowAndroid == null;
        assert mEmbedderActivityContext == null;
        mWindowAndroid = windowAndroid;
        mEmbedderActivityContext = embedderAppContext;
        mViewController = new BrowserViewController(
                windowAndroid, this, mViewControllerState, mFragmentStoppedForConfigurationChange);
        mLocaleReceiver = new LocaleChangedBroadcastReceiver(windowAndroid.getContext().get());
        mPasswordEchoEnabled = null;
    }

    public void onFragmentAttached(
            Context embedderAppContext, FragmentWindowAndroid windowAndroid) {
        createAttachmentState(embedderAppContext, windowAndroid);
        updateAllTabsAndSetActive();
    }

    public void onFragmentDetached() {
        destroyAttachmentState();
        updateAllTabs();
    }

    public void onSaveInstanceState(Bundle outState) {
        boolean hasPersistenceId = !BrowserImplJni.get().getPersistenceId(mNativeBrowser).isEmpty();
        if (mProfile.isIncognito() && hasPersistenceId) {
            // Trigger a save now as saving may generate a new crypto key. This doesn't actually
            // save synchronously, rather triggers a save on a background task runner.
            BrowserImplJni.get().saveBrowserPersisterIfNecessary(mNativeBrowser);
            outState.putByteArray(SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY,
                    BrowserImplJni.get().getBrowserPersisterCryptoKey(mNativeBrowser));
        } else if (!hasPersistenceId) {
            outState.putByteArray(SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY,
                    BrowserImplJni.get().getMinimalPersistenceState(mNativeBrowser));
        }

        if (mWindowAndroid != null) {
            mWindowAndroid.saveInstanceState(outState);
        }
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mWindowAndroid != null) {
            mWindowAndroid.onActivityResult(requestCode, resultCode, data);
        }
    }

    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (mWindowAndroid != null) {
            mWindowAndroid.handlePermissionResult(requestCode, permissions, grantResults);
        }
    }

    @Override
    public void setTopView(IObjectWrapper viewWrapper) {
        StrictModeWorkaround.apply();
        getViewController().setTopView(ObjectWrapper.unwrap(viewWrapper, View.class));
    }

    @Override
    public void setTopViewAndScrollingBehavior(IObjectWrapper viewWrapper, int minHeight,
            boolean onlyExpandControlsAtPageTop, boolean animate) {
        StrictModeWorkaround.apply();
        if (minHeight < 0) {
            throw new IllegalArgumentException("Top view min height must be non-negative.");
        }

        getViewController().setTopControlsAnimationsEnabled(animate);
        getViewController().setTopView(ObjectWrapper.unwrap(viewWrapper, View.class));
        getViewController().setTopControlsMinHeight(minHeight);
        getViewController().setOnlyExpandTopControlsAtPageTop(onlyExpandControlsAtPageTop);
    }

    @Override
    public void setBottomView(IObjectWrapper viewWrapper) {
        StrictModeWorkaround.apply();
        getViewController().setBottomView(ObjectWrapper.unwrap(viewWrapper, View.class));
    }

    @Override
    public TabImpl createTab() {
        TabImpl tab = new TabImpl(mProfile, mWindowAndroid);
        addTab(tab);
        return tab;
    }

    @Override
    public void setSupportsEmbedding(boolean enable, IObjectWrapper valueCallback) {
        StrictModeWorkaround.apply();
        getViewController().setSupportsEmbedding(enable,
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(valueCallback, ValueCallback.class));
    }

    // Only call this if it's guaranteed that Browser is attached to an activity.
    @NonNull
    public BrowserViewController getViewController() {
        if (mViewController == null) {
            throw new RuntimeException("Currently Tab requires Activity context, so "
                    + "it exists only while BrowserFragment is attached to an Activity");
        }
        return mViewController;
    }

    // Can be null in the middle of destroy, or if fragment is detached from activity.
    @Nullable
    public BrowserViewController getPossiblyNullViewController() {
        return mViewController;
    }

    public Context getContext() {
        if (mWindowAndroid == null) {
            return null;
        }

        return mWindowAndroid.getContext().get();
    }

    public boolean isWindowOnSmallDevice() {
        assert mWindowAndroid != null;
        return !DeviceFormFactor.isWindowOnTablet(mWindowAndroid);
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
        TabImpl tab = (TabImpl) iTab;
        if (tab.getBrowser() == this) return;
        BrowserImplJni.get().addTab(mNativeBrowser, tab.getNativeTab());
    }

    @CalledByNative
    private void createJavaTabForNativeTab(long nativeTab) {
        new TabImpl(mProfile, mWindowAndroid, nativeTab);
    }

    private void checkPreferences() {
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
        if (mFontScale != null) {
            float oldFontScale = mFontScale;
            mFontScale = null;
            float newFontScale = getFontScale();
            changed = changed || oldFontScale != newFontScale;
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
        if (mEmbedderActivityContext == null) return false;
        if (mDarkThemeEnabled == null) {
            if (mEmbedderActivityContext == null) return false;
            int uiMode = mEmbedderActivityContext.getResources().getConfiguration().uiMode;
            mDarkThemeEnabled =
                    (uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        }
        return mDarkThemeEnabled;
    }

    @CalledByNative
    private float getFontScale() {
        Context context = getContext();
        if (context == null) return 1.0f;
        if (mFontScale == null) {
            mFontScale = context.getResources().getConfiguration().fontScale;
        }
        return mFontScale;
    }

    Context getEmbedderActivityContext() {
        return mEmbedderActivityContext;
    }

    @CalledByNative
    private void onTabAdded(TabImpl tab) {
        tab.attachToBrowser(this);
        try {
            if (mClient != null) mClient.onTabAdded(tab);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void onActiveTabChanged(TabImpl tab) {
        if (mViewController != null) mViewController.setActiveTab(tab);
        if (mInDestroy) return;
        try {
            if (mClient != null) {
                mClient.onActiveTabChanged(tab != null ? tab.getId() : 0);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void onTabRemoved(TabImpl tab) {
        if (mInDestroy) return;
        try {
            if (mClient != null) mClient.onTabRemoved(tab.getId());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
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
        if (mViewController == null) return false;
        return mViewController.compositorHasSurface();
    }

    @Override
    public boolean setActiveTab(ITab controller) {
        StrictModeWorkaround.apply();
        TabImpl tab = (TabImpl) controller;
        if (tab != null && tab.getBrowser() != this) return false;
        BrowserImplJni.get().setActiveTab(mNativeBrowser, tab != null ? tab.getNativeTab() : 0);
        return true;
    }

    public @Nullable TabImpl getActiveTab() {
        return BrowserImplJni.get().getActiveTab(mNativeBrowser);
    }

    @Override
    public List getTabs() {
        StrictModeWorkaround.apply();
        return Arrays.asList(BrowserImplJni.get().getTabs(mNativeBrowser));
    }

    @Override
    public int getActiveTabId() {
        StrictModeWorkaround.apply();
        return getActiveTab() != null ? getActiveTab().getId() : 0;
    }

    @Override
    public void setClient(IBrowserClient client) {
        StrictModeWorkaround.apply();
        mClient = client;

        // This function is called from the client once everything has been setup (meaning all the
        // client classes have been created and AIDL interfaces established in both directions).
        // This function is called immediately after the constructor of BrowserImpl from the client.
        assert mPersistenceInfo != null;
        PersistenceInfo persistenceInfo = mPersistenceInfo;
        mPersistenceInfo = null;
        BrowserImplJni.get().restoreStateIfNecessary(mNativeBrowser, persistenceInfo.mPersistenceId,
                persistenceInfo.mCryptoKey, persistenceInfo.mMinimalPersistenceState);

        if (getTabs().size() > 0) {
            updateAllTabsAndSetActive();
        } else if (persistenceInfo.mPersistenceId == null
                || persistenceInfo.mPersistenceId.isEmpty()) {
            boolean setActiveResult = setActiveTab(createTab());
            assert setActiveResult;
        } // else case is session restore, which will asynchronously create tabs.
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
    public IUrlBarController getUrlBarController() {
        StrictModeWorkaround.apply();
        return mUrlBarController;
    }

    @Override
    public boolean isRestoringPreviousState() {
        return BrowserImplJni.get().isRestoringPreviousState(mNativeBrowser);
    }

    @CalledByNative
    private void onRestoreCompleted() throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() >= 87) mClient.onRestoreCompleted();
    }

    public View getFragmentView() {
        return getViewController().getView();
    }

    public void destroy() {
        mInDestroy = true;
        BrowserImplJni.get().prepareForShutdown(mNativeBrowser);
        setActiveTab(null);
        for (Object tab : getTabs()) {
            destroyTabImpl((TabImpl) tab);
        }
        destroyAttachmentState();

        // mUrlBarController keeps a reference to mNativeBrowser, and hence must be destroyed before
        // mNativeBrowser.
        mUrlBarController.destroy();
        BrowserImplJni.get().deleteBrowser(mNativeBrowser);

        if (--sInstanceCount == 0) {
            WebLayerAccessibilityUtil.get().onAllBrowsersDestroyed();
        }
    }

    public void onFragmentStart() {
        mFragmentStoppedForConfigurationChange = false;
        mFragmentStarted = true;
        BrowserImplJni.get().onFragmentStart(mNativeBrowser);
        updateAllTabs();
        checkPreferences();
    }

    public void onFragmentStop(boolean forConfigurationChange) {
        mFragmentStoppedForConfigurationChange = forConfigurationChange;
        mFragmentStarted = false;
        updateAllTabs();
    }

    public void onFragmentResume() {
        mFragmentResumed = true;
        WebLayerAccessibilityUtil.get().onBrowserResumed();
        BrowserImplJni.get().onFragmentResume(mNativeBrowser);
    }

    public void onFragmentPause() {
        mFragmentResumed = false;
        BrowserImplJni.get().onFragmentPause(mNativeBrowser);
    }

    public boolean isStarted() {
        return mFragmentStarted;
    }

    public boolean isResumed() {
        return mFragmentResumed;
    }

    public boolean isFragmentStoppedForConfigurationChange() {
        return mFragmentStoppedForConfigurationChange;
    }

    public boolean isViewAttachedToWindow() {
        return mViewAttachedToWindow;
    }

    @Override
    public void onViewAttachedToWindow(View v) {
        mViewAttachedToWindow = true;
        updateAllTabsViewAttachedState();
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        // Note this separate state is needed because v.isAttachedToWindow()
        // still returns true inside this call.
        mViewAttachedToWindow = false;
        updateAllTabsViewAttachedState();
    }

    public MediaRouteDialogFragmentImpl createMediaRouteDialogFragment() {
        try {
            return MediaRouteDialogFragmentImpl.fromRemoteFragment(
                    mClient.createMediaRouteDialogFragment());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private void updateAllTabsViewAttachedState() {
        for (Object tab : getTabs()) {
            ((TabImpl) tab).updateViewAttachedStateFromBrowser();
        }
    }

    private void destroyAttachmentState() {
        if (mLocaleReceiver != null) {
            mLocaleReceiver.destroy();
            mLocaleReceiver = null;
        }
        if (mViewController != null) {
            mViewControllerState = mViewController.getState();
            mViewController.destroy();
            mViewController = null;
            mViewAttachedToWindow = false;
            updateAllTabsViewAttachedState();
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
            mEmbedderActivityContext = null;
        }

        mVisibleSecurityStateObservers.clear();
    }

    private void updateAllTabsAndSetActive() {
        if (getTabs().size() > 0) {
            updateAllTabs();
            mViewController.setActiveTab(getActiveTab());
        }
    }

    private void updateAllTabs() {
        for (Object tab : getTabs()) {
            ((TabImpl) tab).updateFromBrowser();
        }
    }

    @NativeMethods
    interface Natives {
        long createBrowser(long profile, BrowserImpl caller);
        void deleteBrowser(long browser);
        void addTab(long nativeBrowserImpl, long nativeTab);
        TabImpl[] getTabs(long nativeBrowserImpl);
        void setActiveTab(long nativeBrowserImpl, long nativeTab);
        TabImpl getActiveTab(long nativeBrowserImpl);
        void prepareForShutdown(long nativeBrowserImpl);
        String getPersistenceId(long nativeBrowserImpl);
        void saveBrowserPersisterIfNecessary(long nativeBrowserImpl);
        byte[] getBrowserPersisterCryptoKey(long nativeBrowserImpl);
        byte[] getMinimalPersistenceState(long nativeBrowserImpl);
        void restoreStateIfNecessary(long nativeBrowserImpl, String persistenceId,
                byte[] persistenceCryptoKey, byte[] minimalPersistenceState);
        void webPreferencesChanged(long nativeBrowserImpl);
        void onFragmentStart(long nativeBrowserImpl);
        void onFragmentResume(long nativeBrowserImpl);
        void onFragmentPause(long nativeBrowserImpl);
        boolean isRestoringPreviousState(long nativeBrowserImpl);
    }
}
