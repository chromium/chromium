// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.view.LayoutInflater;
import android.view.SurfaceControlViewHost.SurfacePackage;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.concurrent.futures.CallbackToFutureAdapter;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModel;
import androidx.lifecycle.ViewModelProvider;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegateClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Fragment for rendering web content.
 * This is created through `Browser`, since the browsing sandbox must be initialized to render web
 * content.
 */
public class BrowserFragment extends Fragment {
    private SurfaceView mSurfaceView;
    private Browser mBrowser;
    private IBrowserFragmentDelegate mDelegate;
    private final TabListObserverDelegate mTabListObserverDelegate = new TabListObserverDelegate();
    private ListenableFuture<TabManager> mFutureTabManager;
    private CallbackToFutureAdapter.Completer<TabManager> mTabManagerCompleter;
    private TabManager mTabManager;
    private Bundle mInstanceState = new Bundle();

    private final IBrowserFragmentDelegateClient mClient =
            new IBrowserFragmentDelegateClient.Stub() {
                @Override
                public void onSurfacePackageReady(SurfacePackage surfacePackage) {
                    SurfaceView surfaceView = (SurfaceView) BrowserFragment.super.getView();
                    surfaceView.setChildSurfacePackage(surfacePackage);
                }

                @Override
                public void onStarted(Bundle instanceState) {
                    mInstanceState = instanceState;
                    mTabManager = new TabManager(mDelegate);
                    mTabManagerCompleter.set(mTabManager);
                }

                @Override
                public void onContentViewRenderViewReady(
                        IObjectWrapper wrappedContentViewRenderView) {
                    LinearLayout layout = (LinearLayout) BrowserFragment.super.getView();
                    layout.addView(ObjectWrapper.unwrap(wrappedContentViewRenderView, View.class));
                }
            };

    private SurfaceHolder.Callback mSurfaceHolderCallback = new SurfaceHolder.Callback() {
        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            IBinder hostToken = ((SurfaceView) getView()).getHostToken();
            assert hostToken != null;
            try {
                mDelegate.attachViewHierarchy(hostToken);
            } catch (RemoteException e) {
            }
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            resizeSurfaceView(width, height);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {}
    };

    /**
     * This constructor is for the system FragmentManager only. Please use
     * {@link Browser#createFragment}.
     */
    public BrowserFragment() {
        mFutureTabManager = CallbackToFutureAdapter.getFuture(completer -> {
            mTabManagerCompleter = completer;
            // Debug string.
            return "TabManager Future";
        });
    }

    void initialize(Browser browser, IBrowserFragmentDelegate delegate) throws RemoteException {
        mBrowser = browser;
        mDelegate = delegate;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        BrowserViewModel model = getViewModel();
        if (model.hasSavedState()) {
            // Load from view model.
            assert mBrowser == null;

            mBrowser = model.mBrowser;
            mDelegate = model.mDelegate;
        } else {
            // Save to View model.
            assert mBrowser != null;

            model.mBrowser = mBrowser;
            model.mDelegate = mDelegate;
        }

        if (mBrowser.isShutdown()) {
            // This is likely due to an inactive fragment being attached after the Browser Sandbox
            // has been killed.
            invalidate();
            return;
        }

        mBrowser.addFragment(this);

        AppCompatDelegate.create(getActivity(), null);

        try {
            mDelegate.setClient(mClient);
            mDelegate.setTabListObserverDelegate(mTabListObserverDelegate);
            if (Browser.isInProcessMode(context)) {
                // Pass the activity context for the in-process mode.
                // This is because the Autofill Manager is only available with activity contexts.
                // This will be cleaned up when the fragment is detached.
                mDelegate.onAttachWithContext(ObjectWrapper.wrap(context));
            } else {
                mDelegate.onAttach();
            }
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            mDelegate.onCreate(savedInstanceState);
        } catch (RemoteException e) {
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        if (Browser.isInProcessMode(getActivity())) {
            LinearLayout layout = new LinearLayout(getActivity());
            try {
                mDelegate.retrieveContentViewRenderView();
            } catch (RemoteException e) {
            }
            return layout;
        } else {
            return new BrowserSurfaceView(getActivity(), mSurfaceHolderCallback);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // We intentionally do not forward the onDestroy call here to avoid destroying/recreating
        // the WebLayer implementations every time.
        // onDestroy is called once the ViewModel is cleared because that guarantees that the
        // Fragment will no longer be used.
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mBrowser.removeFragment(this);
        try {
            mDelegate.onDetach();
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        try {
            mDelegate.onStart();
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        try {
            mDelegate.onStop();
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        try {
            mDelegate.onResume();
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        try {
            mDelegate.onPause();
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putAll(mInstanceState);
    }

    public Browser getBrowser() {
        return mBrowser;
    }

    private void resizeSurfaceView(int width, int height) {
        try {
            mDelegate.resizeView(width, height);
        } catch (RemoteException e) {
        }
    }

    /**
     * Returns a ListenableFuture to the TabManager, which becomes available after the
     * BrowserFragments onStart method finished.
     */
    @NonNull
    public ListenableFuture<TabManager> getTabManager() {
        return mFutureTabManager;
    }

    /**
     * Registers a browser observer and returns if successful.
     *
     * @param tabListObserver The TabListObserver.
     *
     * @return true if observer was added to the list of observers.
     */
    public boolean registerTabListObserver(@NonNull TabListObserver tabListObserver) {
        return mTabListObserverDelegate.registerObserver(tabListObserver);
    }

    /**
     * Unregisters a browser observer and returns if successful.
     *
     * @param tabListObserver The TabListObserver to remove.
     *
     * @return true if observer was removed from the list of observers.
     */
    public boolean unregisterTabListObserver(@NonNull TabListObserver tabListObserver) {
        return mTabListObserverDelegate.unregisterObserver(tabListObserver);
    }

    private BrowserViewModel getViewModel() {
        return new ViewModelProvider(this).get(BrowserViewModel.class);
    }

    void invalidate() {
        // The fragment is synchronously removed so that the shutdown steps can complete.
        getParentFragmentManager().beginTransaction().remove(this).commitNow();
        mDelegate = null;
        mBrowser = null;
        mFutureTabManager = Futures.immediateFailedFuture(
                new IllegalStateException("Browser has been destroyed"));
        if (mTabManager != null) {
            mTabManager.invalidate();
            mTabManager = null;
        }
    }

    /**
     * A custom SurfaceView that registers a SurfaceHolder.Callback.
     */
    private class BrowserSurfaceView extends SurfaceView {
        private SurfaceHolder.Callback mSurfaceHolderCallback;

        BrowserSurfaceView(Context context, SurfaceHolder.Callback surfaceHolderCallback) {
            super(context);
            mSurfaceHolderCallback = surfaceHolderCallback;
            setZOrderOnTop(true);
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            getHolder().addCallback(mSurfaceHolderCallback);
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            getHolder().removeCallback(mSurfaceHolderCallback);
        }
    }

    /**
     * This class is an implementation detail and not intended for public use. It may change at any
     * time in incompatible ways, including being removed.
     * <p>
     * This class stores BrowserFragment specific state to a ViewModel so that it can reused if a
     * new Fragment is created that should share the same state.
     */
    public static final class BrowserViewModel extends ViewModel {
        @Nullable
        private Browser mBrowser;
        @Nullable
        private IBrowserFragmentDelegate mDelegate;

        boolean hasSavedState() {
            return mBrowser != null;
        }

        @Override
        public void onCleared() {
            if (mDelegate != null) {
                try {
                    mDelegate.onDestroy();
                } catch (RemoteException e) {
                }
            }

            mBrowser = null;
            mDelegate = null;
        }
    }
}
