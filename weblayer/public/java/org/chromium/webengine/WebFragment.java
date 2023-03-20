// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.content.Context;
import android.content.Intent;
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

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModel;
import androidx.lifecycle.ViewModelProvider;

import org.chromium.webengine.interfaces.IWebFragmentEventsDelegate;
import org.chromium.webengine.interfaces.IWebFragmentEventsDelegateClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Fragment for rendering web content. This is obtained through WebEngine.
 */
public class WebFragment extends Fragment {
    private SurfaceView mSurfaceView;
    private WebSandbox mWebSandbox;
    private WebEngine mWebEngine;
    private IWebFragmentEventsDelegate mDelegate;

    private final IWebFragmentEventsDelegateClient mClient =
            new IWebFragmentEventsDelegateClient.Stub() {
                @Override
                public void onSurfacePackageReady(SurfacePackage surfacePackage) {
                    SurfaceView surfaceView = (SurfaceView) WebFragment.super.getView();
                    surfaceView.setChildSurfacePackage(surfacePackage);
                }

                @Override
                public void onContentViewRenderViewReady(
                        IObjectWrapper wrappedContentViewRenderView) {
                    LinearLayout layout = (LinearLayout) WebFragment.super.getView();
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
     * {@link WebSandbox#createFragment}.
     */
    public WebFragment() {}

    void initialize(WebSandbox webSandbox, WebEngine webEngine, IWebFragmentEventsDelegate delegate)
            throws RemoteException {
        mWebSandbox = webSandbox;
        mWebEngine = webEngine;
        mDelegate = delegate;
    }

    /**
     * Returns the {@link WebEngine} associated with this Fragment.
     */
    public WebEngine getWebEngine() {
        return mWebEngine;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        WebViewModel model = getViewModel();
        if (model.hasSavedState()) {
            // Load from view model.
            assert mWebSandbox == null;

            mWebSandbox = model.mWebSandbox;
            mWebEngine = model.mWebEngine;
            mDelegate = model.mDelegate;

            // Recreating a WebFragment e.g. after rotating the device creates a new WebFragment
            // based on the data from the ViewModel. The WebFragment in the WebEngine needs to be
            // updated as it needs to have the current instance.
            mWebEngine.updateFragment(this);
        } else {
            // Save to View model.
            assert mWebSandbox != null;

            model.mWebSandbox = mWebSandbox;
            model.mWebEngine = mWebEngine;
            model.mDelegate = mDelegate;
        }

        if (mWebSandbox.isShutdown()) {
            // This is likely due to an inactive fragment being attached after the Web Sandbox
            // has been killed.
            invalidate();
            return;
        }

        AppCompatDelegate.create(getActivity(), null);

        try {
            mDelegate.setClient(mClient);
            if (WebSandbox.isInProcessMode(context)) {
                // Pass the activity context for the in-process mode.
                // This is because the Autofill Manager is only available with activity contexts.
                // This will be cleaned up when the fragment is detached.
                mDelegate.onAttachWithContext(
                        ObjectWrapper.wrap(context), ObjectWrapper.wrap((Fragment) this));
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
            mDelegate.onCreate();
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
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        try {
            mDelegate.onActivityResult(requestCode, resultCode, ObjectWrapper.wrap(data));
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        try {
            mDelegate.onDestroy();
        } catch (RemoteException e) {
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

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

    /**
     * Returns the Sandbox that created this WebFragment.
     *
     * @return WebSandbox WebSandbox that created this Fragment.
     */
    public WebSandbox getWebSandbox() {
        return mWebSandbox;
    }

    private void resizeSurfaceView(int width, int height) {
        try {
            mDelegate.resizeView(width, height);
        } catch (RemoteException e) {
        }
    }

    private WebViewModel getViewModel() {
        return new ViewModelProvider(this).get(WebViewModel.class);
    }

    void invalidate() {
        try {
            // The fragment is synchronously removed so that the shutdown steps can complete.
            getParentFragmentManager().beginTransaction().remove(this).commitNow();
        } catch (IllegalStateException e) {
            // Fragment already removed from FragmentManager.
            return;
        } finally {
            mDelegate = null;
            mWebEngine = null;
            mWebSandbox = null;
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
        private WebEngine mWebEngine;
        @Nullable
        private IWebFragmentEventsDelegate mDelegate;

        boolean hasSavedState() {
            return mWebSandbox != null;
        }

        @Override
        public void onCleared() {
            mWebSandbox = null;
            mWebEngine = null;
            mDelegate = null;
        }
    }
}
