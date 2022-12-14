// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.ContextThemeWrapper;
import android.view.SurfaceControlViewHost;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Implementation of RemoteFragmentImpl which forwards logic to BrowserImpl.
 */
public class BrowserFragmentImpl extends FragmentHostingRemoteFragmentImpl {
    private static int sResumedCount;
    private static long sSessionStartTimeMs;

    private final ProfileImpl mProfile;
    private final String mPersistenceId;

    private int mMinimumSurfaceWidth;
    private int mMinimumSurfaceHeight;

    private BrowserImpl mBrowser;

    // The embedder's original context object. Only use this to resolve resource IDs provided by the
    // embedder.
    private Context mEmbedderActivityContext;

    public BrowserFragmentImpl(ProfileManager profileManager, Bundle fragmentArgs) {
        super();
        mPersistenceId = fragmentArgs.getString(BrowserFragmentArgs.PERSISTENCE_ID);
        String name = fragmentArgs.getString(BrowserFragmentArgs.PROFILE_NAME);

        boolean isIncognito;
        if (fragmentArgs.containsKey(BrowserFragmentArgs.IS_INCOGNITO)) {
            isIncognito = fragmentArgs.getBoolean(BrowserFragmentArgs.IS_INCOGNITO, false);
        } else {
            isIncognito = "".equals(name);
        }
        mProfile = profileManager.getProfile(name, isIncognito);
    }

    @Override
    protected void onAttach(Context context) {
        StrictModeWorkaround.apply();
        super.onAttach(context);
        mEmbedderActivityContext = context;
        if (mBrowser != null) { // On first creation, onAttach is called before onCreate
            mBrowser.onFragmentAttached(mEmbedderActivityContext,
                    new FragmentWindowAndroid(getWebLayerContext(), this));
            mBrowser.getViewController().setMinimumSurfaceSize(
                    mMinimumSurfaceWidth, mMinimumSurfaceHeight);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        StrictModeWorkaround.apply();
        super.onCreate(savedInstanceState);
        // onCreate() is only called once
        assert mBrowser == null;
        // onCreate() is always called after onAttach(). onAttach() sets |getWebLayerContext()| and
        // |mEmbedderContext|.
        assert getWebLayerContext() != null;
        assert mEmbedderActivityContext != null;
        mBrowser = new BrowserImpl(mEmbedderActivityContext, mProfile, mPersistenceId,
                savedInstanceState, new FragmentWindowAndroid(getWebLayerContext(), this));
    }

    @Override
    protected View onCreateView(ViewGroup container, Bundle savedInstanceState) {
        StrictModeWorkaround.apply();
        return mBrowser.getFragmentView();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        StrictModeWorkaround.apply();
        mBrowser.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    protected void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        StrictModeWorkaround.apply();
        mBrowser.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @Override
    protected void onDestroy() {
        StrictModeWorkaround.apply();
        super.onDestroy();
        mBrowser.destroy();
        mBrowser = null;
    }

    @Override
    protected void onDetach() {
        StrictModeWorkaround.apply();
        super.onDetach();
        // mBrowser != null if fragment is retained, otherwise onDestroy is called first.
        if (mBrowser != null) {
            mBrowser.onFragmentDetached();
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        StrictModeWorkaround.apply();
        mBrowser.onSaveInstanceState(outState);
        super.onSaveInstanceState(outState);
    }

    @Override
    protected void onStart() {
        super.onStart();
        mBrowser.onFragmentStart();
    }

    @Override
    protected void onStop() {
        super.onStop();
        Activity activity = getActivity();
        mBrowser.onFragmentStop(activity != null && activity.getChangingConfigurations() != 0);
    }

    @Override
    protected void onResume() {
        super.onResume();
        sResumedCount++;
        if (sResumedCount == 1) sSessionStartTimeMs = SystemClock.uptimeMillis();
        mBrowser.onFragmentResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        sResumedCount--;
        if (sResumedCount == 0) {
            long deltaMs = SystemClock.uptimeMillis() - sSessionStartTimeMs;
            RecordHistogram.recordLongTimesHistogram("Session.TotalDuration", deltaMs);
        }
        mBrowser.onFragmentPause();
    }

    @RequiresApi(Build.VERSION_CODES.R)
    @Override
    protected void setSurfaceControlViewHost(SurfaceControlViewHost host) {
        // TODO(rayankans): Handle fallback for older devices.
        host.setView(mBrowser.getViewController().getView(), 0, 0);
    }

    @Override
    protected View getContentViewRenderView() {
        return mBrowser.getViewController().getView();
    }

    @Override
    protected void setMinimumSurfaceSize(int width, int height) {
        StrictModeWorkaround.apply();
        mMinimumSurfaceWidth = width;
        mMinimumSurfaceHeight = height;
        BrowserViewController viewController = mBrowser.getPossiblyNullViewController();
        if (viewController == null) return;
        viewController.setMinimumSurfaceSize(width, height);
    }

    @Nullable
    public BrowserImpl getBrowser() {
        return mBrowser;
    }

    public IBrowserFragment asIBrowserFragment() {
        return new IBrowserFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return BrowserFragmentImpl.this;
            }

            @Override
            public IBrowser getBrowser() {
                StrictModeWorkaround.apply();
                if (mBrowser == null) {
                    throw new RuntimeException("Browser is available only between"
                            + " BrowserFragment's onCreate() and onDestroy().");
                }
                return mBrowser;
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
