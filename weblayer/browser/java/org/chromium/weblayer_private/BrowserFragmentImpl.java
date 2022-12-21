// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;
import android.view.ContextThemeWrapper;
import android.view.SurfaceControlViewHost;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Implementation of RemoteFragmentImpl which provides the Fragment implementation for BrowserImpl.
 */
public class BrowserFragmentImpl extends FragmentHostingRemoteFragmentImpl {
    private static int sResumedCount;
    private static long sSessionStartTimeMs;

    private BrowserImpl mBrowser;

    private BrowserViewController mViewController;

    private LocaleChangedBroadcastReceiver mLocaleReceiver;

    private boolean mViewAttachedToWindow;

    private int mMinimumSurfaceWidth;
    private int mMinimumSurfaceHeight;

    private FragmentWindowAndroid mWindowAndroid;
    private Context mEmbedderContext;

    // Tracks whether the fragment is in the middle of a configuration change when stopped. During
    // a configuration change the fragment goes through a full lifecycle and usually the webContents
    // is hidden when detached and shown again when re-attached. Tracking the configuration change
    // allows continuing to show the WebContents (still following all other lifecycle events as
    // normal), which allows continuous playback of videos.
    private boolean mInConfigurationChangeAndWasAttached;

    /**
     * @param windowAndroid a window that was created by a {@link BrowserFragmentImpl}. It's not
     *         valid to call this method with other {@link WindowAndroid} instances. Typically this
     *         should be the {@link WindowAndroid} of a {@link WebContents}.
     * @return the associated BrowserImpl instance.
     */
    public static BrowserFragmentImpl fromWindowAndroid(WindowAndroid windowAndroid) {
        assert windowAndroid instanceof FragmentWindowAndroid;
        return ((FragmentWindowAndroid) windowAndroid).getFragment();
    }

    public BrowserFragmentImpl(BrowserImpl browser, Context context) {
        super(context);

        mBrowser = browser;
        mWindowAndroid = new FragmentWindowAndroid(getWebLayerContext(), this);
    }

    private void createAttachmentState(Context embedderContext) {
        assert mViewController == null;

        mViewController = new BrowserViewController(embedderContext, mWindowAndroid, false);
        mViewController.setMinimumSurfaceSize(mMinimumSurfaceWidth, mMinimumSurfaceHeight);
        mViewAttachedToWindow = true;

        mLocaleReceiver = new LocaleChangedBroadcastReceiver(mWindowAndroid.getContext().get());
    }

    private void destroyAttachmentState() {
        if (mLocaleReceiver != null) {
            mLocaleReceiver.destroy();
            mLocaleReceiver = null;
        }
        if (mViewController != null) {
            mViewController.destroy();
            mViewController = null;
            mViewAttachedToWindow = false;
            mBrowser.updateAllTabsViewAttachedState();
        }
    }

    @Override
    protected void onCreate() {
        StrictModeWorkaround.apply();
        super.onCreate();
    }

    @Override
    protected void onStart() {
        StrictModeWorkaround.apply();
        super.onStart();
        mBrowser.notifyFragmentInit();
        mBrowser.updateAllTabs();
    }

    @Override
    protected void onPause() {
        super.onPause();
        sResumedCount--;
        if (sResumedCount == 0) {
            long deltaMs = SystemClock.uptimeMillis() - sSessionStartTimeMs;
            RecordHistogram.recordLongTimesHistogram("Session.TotalDuration", deltaMs);
        }
        mBrowser.notifyFragmentPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        sResumedCount++;
        if (sResumedCount == 1) sSessionStartTimeMs = SystemClock.uptimeMillis();
        mBrowser.notifyFragmentResume();
    }

    @Override
    protected void onAttach(Context embedderContext) {
        StrictModeWorkaround.apply();
        super.onAttach(embedderContext);
        mEmbedderContext = embedderContext;

        mInConfigurationChangeAndWasAttached = false;

        setMinimumSurfaceSize(mMinimumSurfaceWidth, mMinimumSurfaceHeight);

        createAttachmentState(embedderContext);

        mBrowser.updateAllTabs();
        setActiveTab(mBrowser.getActiveTab());
        mBrowser.checkPreferences();
    }

    @Override
    protected void onDetach() {
        StrictModeWorkaround.apply();
        super.onDetach();
        mEmbedderContext = null;
        destroyAttachmentState();
        mBrowser.updateAllTabs();
    }

    @Override
    protected void onStop() {
        super.onStop();
        Activity activity = ContextUtils.activityFromContext(mEmbedderContext);
        if (activity != null) {
            mInConfigurationChangeAndWasAttached = activity.getChangingConfigurations() != 0;
        }

        mBrowser.updateAllTabs();
    }

    @Override
    protected void onDestroy() {
        StrictModeWorkaround.apply();
        super.onDestroy();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        StrictModeWorkaround.apply();
        if (mWindowAndroid != null) {
            IntentRequestTracker tracker = mWindowAndroid.getIntentRequestTracker();
            assert tracker != null;
            tracker.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    protected void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        StrictModeWorkaround.apply();
        if (mWindowAndroid != null) {
            mWindowAndroid.handlePermissionResult(requestCode, permissions, grantResults);
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    @Override
    protected void setSurfaceControlViewHost(SurfaceControlViewHost host) {
        // TODO(rayankans): Handle fallback for older devices.
        host.setView(getViewController().getView(), 0, 0);
    }

    @Override
    protected View getContentViewRenderView() {
        return getViewController().getView();
    }

    @Override
    public void setMinimumSurfaceSize(int width, int height) {
        StrictModeWorkaround.apply();
        mMinimumSurfaceWidth = width;
        mMinimumSurfaceHeight = height;
        if (mViewController == null) return;
        mViewController.setMinimumSurfaceSize(width, height);
    }

    // Only call this if it's guaranteed that Browser is attached to an activity.
    @NonNull
    public BrowserViewController getViewController() {
        if (mViewController == null) {
            throw new RuntimeException("Currently Tab requires Activity context, so "
                    + "it exists only while WebFragment is attached to an Activity");
        }
        return mViewController;
    }

    @Nullable
    public BrowserViewController getPossiblyNullViewController() {
        return mViewController;
    }

    public BrowserImpl getBrowser() {
        return mBrowser;
    }

    void setActiveTab(TabImpl tab) {
        if (mViewController == null) return;
        mViewController.setActiveTab(tab);
    }

    boolean compositorHasSurface() {
        if (mViewController == null) return false;
        return mViewController.compositorHasSurface();
    }

    @Nullable
    ContentView getViewAndroidDelegateContainerView() {
        if (mViewController == null) return null;
        return mViewController.getContentView();
    }

    FragmentWindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    boolean isAttached() {
        return mViewAttachedToWindow;
    }

    /**
     * Returns true if the Fragment should be considered visible.
     */
    boolean isVisible() {
        return mInConfigurationChangeAndWasAttached || mViewAttachedToWindow;
    }

    void shutdown() {
        destroyAttachmentState();

        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
        }
    }

    public IBrowserFragment asIBrowserFragment() {
        return new IBrowserFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return BrowserFragmentImpl.this;
            }
        };
    }

    @Override
    protected FragmentHostingRemoteFragmentImpl.RemoteFragmentContext createRemoteFragmentContext(
            Context embedderContext) {
        Context wrappedContext = ClassLoaderContextWrapperFactory.get(embedderContext);
        Context themedContext =
                new ContextThemeWrapper(wrappedContext, R.style.Theme_WebLayer_Settings);
        return new FragmentHostingRemoteFragmentImpl.RemoteFragmentContext(themedContext);
    }
}
