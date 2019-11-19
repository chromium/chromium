// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.IntentSender.SendIntentException;
import android.os.Bundle;
import android.os.RemoteException;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.concurrent.Future;

/**
 * WebLayer's fragment implementation.
 *
 * This class is an implementation detail and will eventually be hidden. Use
 * {@link Browser#fromFragment} to get the Browser from a
 * Fragment created by WebLayer.
 *
 * All the browser APIs, such as loading pages can be accessed via
 * {@link Browser}, which can be retrieved with {@link
 * FragmentSupport#getBrowserForFragment} after the fragment received onCreate the
 * call.
 *
 * Attaching a BrowserFragment to an Activity requires WebLayer to be initialized, so
 * BrowserFragment will block the thread in onAttach until it's done. To prevent this,
 * asynchronously "pre-warm" WebLayer using {@link WebLayer#create} prior to using BrowserFragments.
 *
 * Unfortunately, when the system restores the BrowserFragment after killing the process, it
 * attaches the fragment immediately on activity's onCreate event, so there is currently no way to
 * asynchronously init WebLayer in that case.
 */
public final class BrowserFragment extends Fragment {
    private final IRemoteFragmentClient mClientImpl = new IRemoteFragmentClient.Stub() {
        @Override
        public void superOnCreate(IObjectWrapper savedInstanceState) {
            BrowserFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnAttach(IObjectWrapper context) {
            BrowserFragment.super.onAttach(ObjectWrapper.unwrap(context, Context.class));
        }

        @Override
        public void superOnActivityCreated(IObjectWrapper savedInstanceState) {
            BrowserFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnStart() {
            BrowserFragment.super.onStart();
        }

        @Override
        public void superOnResume() {
            BrowserFragment.super.onResume();
        }

        @Override
        public void superOnPause() {
            BrowserFragment.super.onPause();
        }

        @Override
        public void superOnStop() {
            BrowserFragment.super.onStop();
        }

        @Override
        public void superOnDestroyView() {
            BrowserFragment.super.onDestroyView();
        }

        @Override
        public void superOnDetach() {
            BrowserFragment.super.onDetach();
        }

        @Override
        public void superOnDestroy() {
            BrowserFragment.super.onDestroy();
        }

        @Override
        public void superOnSaveInstanceState(IObjectWrapper outState) {
            BrowserFragment.super.onSaveInstanceState(ObjectWrapper.unwrap(outState, Bundle.class));
        }

        @Override
        public IObjectWrapper getActivity() {
            return ObjectWrapper.wrap(BrowserFragment.this.getActivity());
        }

        @Override
        public boolean startActivityForResult(
                IObjectWrapper intent, int requestCode, IObjectWrapper options) {
            try {
                BrowserFragment.this.startActivityForResult(
                        ObjectWrapper.unwrap(intent, Intent.class), requestCode,
                        ObjectWrapper.unwrap(options, Bundle.class));
            } catch (ActivityNotFoundException e) {
                return false;
            }
            return true;
        }

        @Override
        public boolean startIntentSenderForResult(IObjectWrapper intent, int requestCode,
                IObjectWrapper fillInIntent, int flagsMask, int flagsValues, int extraFlags,
                IObjectWrapper options) {
            try {
                BrowserFragment.this.startIntentSenderForResult(
                        ObjectWrapper.unwrap(intent, IntentSender.class), requestCode,
                        ObjectWrapper.unwrap(fillInIntent, Intent.class), flagsMask, flagsValues,
                        extraFlags, ObjectWrapper.unwrap(options, Bundle.class));
            } catch (SendIntentException e) {
                return false;
            }
            return true;
        }

        @Override
        public boolean shouldShowRequestPermissionRationale(String permission) {
            return BrowserFragment.this.shouldShowRequestPermissionRationale(permission);
        }

        @Override
        public void requestPermissions(String[] permissions, int requestCode) {
            BrowserFragment.this.requestPermissions(permissions, requestCode);
        }
    };

    // Nonnull after first onAttach().
    private IBrowserFragment mImpl;
    private IRemoteFragment mRemoteFragment;
    private WebLayer mWebLayer;

    // Nonnull between onCreate() and onDestroy().
    private Browser mBrowser;

    /**
     * This constructor is for the system FragmentManager only. Please use
     * {@link WebLayer#createBrowserFragment}.
     */
    public BrowserFragment() {
        super();
        ThreadCheck.ensureOnUiThread();
    }

    /**
     * Returns the {@link Browser} associated with this fragment.
     * The browser is available only between BrowserFragment's onCreate() and onDestroy().
     */
    @NonNull
    Browser getBrowser() {
        ThreadCheck.ensureOnUiThread();
        if (mBrowser == null) {
            throw new RuntimeException("Browser is available only between "
                    + "BrowserFragment's onCreate() and onDestroy().");
        }
        return mBrowser;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnActivityResult(
                    requestCode, resultCode, ObjectWrapper.wrap(data));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnRequestPermissionsResult(
                    requestCode, permissions, grantResults);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onAttach(Context context) {
        ThreadCheck.ensureOnUiThread();
        // This is the first lifecycle event and also the first time we can get app context (unless
        // the embedder has already called getBrowser()). So it's the latest and at the same time
        // the earliest moment when we can initialize WebLayer without missing any lifecycle events.
        ensureConnectedToWebLayer(context.getApplicationContext());
        try {
            mRemoteFragment.handleOnAttach(ObjectWrapper.wrap(context));
            // handleOnAttach results in creating BrowserImpl on the other side.
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private void ensureConnectedToWebLayer(Context appContext) {
        if (mImpl != null) {
            return; // Already initialized.
        }
        Bundle args = getArguments();
        if (args == null) {
            throw new RuntimeException("BrowserFragment was created without arguments.");
        }
        try {
            Future<WebLayer> future = WebLayer.create(appContext);
            mWebLayer = future.get();
            mImpl = mWebLayer.connectFragment(mClientImpl, args);
            mRemoteFragment = mImpl.asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnCreate(ObjectWrapper.wrap(savedInstanceState));
            mBrowser = new Browser(mImpl.getBrowser());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        ThreadCheck.ensureOnUiThread();
        try {
            return ObjectWrapper.unwrap(mRemoteFragment.handleOnCreateView(), View.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnActivityCreated(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStart() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnStart();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onResume() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnResume();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onSaveInstanceState(Bundle outState) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnSaveInstanceState(ObjectWrapper.wrap(outState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onPause() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnPause();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStop() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnStop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroyView() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnDestroyView();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroy() {
        ThreadCheck.ensureOnUiThread();
        mBrowser.prepareForDestroy();
        try {
            mRemoteFragment.handleOnDestroy();
            // The other side does the clean up automatically in handleOnDestroy()
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        mBrowser = null;
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDetach() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnDetach();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
