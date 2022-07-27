// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;
import android.view.LayoutInflater;
import android.view.SurfaceControlViewHost.SurfacePackage;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.fragment.app.Fragment;

import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegateClient;

/**
 * Fragment for rendering web content.
 * This is created through `Browser`, since the browsing sandbox must be initialized to render web
 * content.
 */
public class BrowserFragment extends Fragment {
    private SurfaceView mSurfaceView;
    private Browser mBrowser;
    private IBrowserFragmentDelegate mDelegate;

    private final IBrowserFragmentDelegateClient mClient =
            new IBrowserFragmentDelegateClient.Stub() {
                @Override
                public void onSurfacePackageReady(SurfacePackage surfacePackage) {
                    mSurfaceView.setChildSurfacePackage(surfacePackage);
                    // Set initial size and LayoutChangeListener of {@code mSurfaceView}, as {@link
                    // SurfaceControlViewHost} is only initialized now.
                    resizeSurfaceView(mSurfaceView.getWidth(), mSurfaceView.getHeight());
                    mSurfaceView.addOnLayoutChangeListener(
                            (View v, int left, int top, int right, int bottom, int oldLeft,
                                    int oldTop, int oldRight, int oldBottom) -> {
                                resizeSurfaceView(right - left, bottom - top);
                            });
                }
            };

    BrowserFragment(Browser browser, IBrowserFragmentDelegate delegate) throws RemoteException {
        // TODO(rayankans): Create empty constructor and load from ViewModel.
        mBrowser = browser;
        mDelegate = delegate;
        mDelegate.setClient(mClient);
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mSurfaceView = new SurfaceView(context);
        mSurfaceView.setZOrderOnTop(true);

        AppCompatDelegate.create(getActivity(), null);

        try {
            mDelegate.onAttach();
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
        return mSurfaceView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        try {
            mDelegate.attachViewHierarchy(mSurfaceView.getHostToken());
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

    @Override
    public void onSaveInstanceState(Bundle outState) {
        // TODO(rayankans): Synchronously retrieve instance state from delegate.
        super.onSaveInstanceState(outState);
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
}
