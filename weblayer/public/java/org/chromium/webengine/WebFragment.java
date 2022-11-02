// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

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

import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.ITabManagerDelegate;
import org.chromium.webengine.interfaces.IWebFragmentDelegate;
import org.chromium.webengine.interfaces.IWebFragmentDelegateClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Fragment for rendering web content. This is created through `WebSandbox`.
 */
public class WebFragment extends Fragment {
    private SurfaceView mSurfaceView;
    private WebSandbox mWebSandbox;
    private IWebFragmentDelegate mDelegate;
    private final TabListObserverDelegate mTabListObserverDelegate = new TabListObserverDelegate();

    // TabManager
    private ListenableFuture<TabManager> mFutureTabManager;
    private CallbackToFutureAdapter.Completer<TabManager> mTabManagerCompleter;
    private TabManager mTabManager;

    // CookieManager
    private ListenableFuture<CookieManager> mFutureCookieManager;
    private CallbackToFutureAdapter.Completer<CookieManager> mCookieManagerCompleter;
    private CookieManager mCookieManager;

    private Bundle mInstanceState = new Bundle();

    private final IWebFragmentDelegateClient mClient = new IWebFragmentDelegateClient.Stub() {
        @Override
        public void onSurfacePackageReady(SurfacePackage surfacePackage) {
            SurfaceView surfaceView = (SurfaceView) WebFragment.super.getView();
            surfaceView.setChildSurfacePackage(surfacePackage);
        }

        @Override
        public void onStarted(Bundle instanceState) {
            mInstanceState = instanceState;
        }

        @Override
        public void onTabManagerReady(ITabManagerDelegate delegate) {
            mTabManager = new TabManager(delegate);
            mTabManagerCompleter.set(mTabManager);
        }

        @Override
        public void onContentViewRenderViewReady(IObjectWrapper wrappedContentViewRenderView) {
            LinearLayout layout = (LinearLayout) WebFragment.super.getView();
            layout.addView(ObjectWrapper.unwrap(wrappedContentViewRenderView, View.class));
        }

        @Override
        public void onCookieManagerReady(ICookieManagerDelegate delegate) {
            mCookieManager = new CookieManager(delegate);
            mCookieManagerCompleter.set(mCookieManager);
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
     * {@link WebSandbox#createFragment}.
     */
    public WebFragment() {
        mFutureTabManager = CallbackToFutureAdapter.getFuture(completer -> {
            mTabManagerCompleter = completer;
            // Debug string.
            return "TabManager Future";
        });
        mFutureCookieManager = CallbackToFutureAdapter.getFuture(completer -> {
            mCookieManagerCompleter = completer;
            // Debug string.
            return "CookieManager Future";
        });
    }

    void initialize(WebSandbox webSandbox, IWebFragmentDelegate delegate) throws RemoteException {
        mWebSandbox = webSandbox;
        mDelegate = delegate;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        WebViewModel model = getViewModel();
        if (model.hasSavedState()) {
            // Load from view model.
            assert mWebSandbox == null;

            mWebSandbox = model.mWebSandbox;
            mDelegate = model.mDelegate;
        } else {
            // Save to View model.
            assert mWebSandbox != null;

            model.mWebSandbox = mWebSandbox;
            model.mDelegate = mDelegate;
        }

        if (mWebSandbox.isShutdown()) {
            // This is likely due to an inactive fragment being attached after the Web Sandbox
            // has been killed.
            invalidate();
            return;
        }

        mWebSandbox.addFragment(this);

        AppCompatDelegate.create(getActivity(), null);

        try {
            mDelegate.setClient(mClient);
            mDelegate.setTabListObserverDelegate(mTabListObserverDelegate);
            if (WebSandbox.isInProcessMode(context)) {
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
        if (WebSandbox.isInProcessMode(getActivity())) {
            LinearLayout layout = new LinearLayout(getActivity());
            try {
                mDelegate.retrieveContentViewRenderView();
            } catch (RemoteException e) {
            }
            return layout;
        } else {
            return new WebSurfaceView(getActivity(), mSurfaceHolderCallback);
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

        mWebSandbox.removeFragment(this);
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

    public WebSandbox getWebSandbox() {
        return mWebSandbox;
    }

    private void resizeSurfaceView(int width, int height) {
        try {
            mDelegate.resizeView(width, height);
        } catch (RemoteException e) {
        }
    }

    /**
     * Returns a ListenableFuture to the TabManager, which becomes available after the
     * WebFragments onStart method finished.
     */
    @NonNull
    public ListenableFuture<TabManager> getTabManager() {
        return mFutureTabManager;
    }

    /**
     * Returns a ListenableFuture to the CookieManager, which becomes available after the
     * WebFragment's onCreate method finishes.
     */
    @NonNull
    public ListenableFuture<CookieManager> getCookieManager() {
        return mFutureCookieManager;
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

    private WebViewModel getViewModel() {
        return new ViewModelProvider(this).get(WebViewModel.class);
    }

    void invalidate() {
        // The fragment is synchronously removed so that the shutdown steps can complete.
        getParentFragmentManager().beginTransaction().remove(this).commitNow();
        mDelegate = null;
        mWebSandbox = null;
        mFutureTabManager = Futures.immediateFailedFuture(
                new IllegalStateException("WebSandbox has been destroyed"));
        if (mTabManager != null) {
            mTabManager.invalidate();
            mTabManager = null;
        }
        mFutureCookieManager = Futures.immediateFailedFuture(
                new IllegalStateException("WebSandbox has been destroyed"));
        if (mCookieManager != null) {
            mCookieManager.invalidate();
            mCookieManager = null;
        }
    }

    /**
     * A custom SurfaceView that registers a SurfaceHolder.Callback.
     */
    private class WebSurfaceView extends SurfaceView {
        private SurfaceHolder.Callback mSurfaceHolderCallback;

        WebSurfaceView(Context context, SurfaceHolder.Callback surfaceHolderCallback) {
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
     * This class stores WebFragment specific state to a ViewModel so that it can reused if a
     * new Fragment is created that should share the same state.
     */
    public static final class WebViewModel extends ViewModel {
        @Nullable
        private WebSandbox mWebSandbox;
        @Nullable
        private IWebFragmentDelegate mDelegate;

        boolean hasSavedState() {
            return mWebSandbox != null;
        }

        @Override
        public void onCleared() {
            if (mDelegate != null) {
                try {
                    mDelegate.onDestroy();
                } catch (RemoteException e) {
                }
            }

            mWebSandbox = null;
            mDelegate = null;
        }
    }
}
