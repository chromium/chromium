// Copyright 2020 The Chromium Authors. All rights reserved.
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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * A base class for the client side of Fragments that are run in WebLayer's implementation.
 *
 * This class forwards all Fragment lifecycle events over the IRemoteFragment aidl interface.
 */
abstract class RemoteFragment extends Fragment {
    private final IRemoteFragmentClient mClientImpl = new IRemoteFragmentClient.Stub() {
        @Override
        public void superOnCreate(IObjectWrapper savedInstanceState) {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnAttach(IObjectWrapper context) {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onAttach(ObjectWrapper.unwrap(context, Context.class));
        }

        @Override
        public void superOnActivityCreated(IObjectWrapper savedInstanceState) {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnStart() {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onStart();
        }

        @Override
        public void superOnResume() {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onResume();
        }

        @Override
        public void superOnPause() {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onPause();
        }

        @Override
        public void superOnStop() {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onStop();
        }

        @Override
        public void superOnDestroyView() {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onDestroyView();
        }

        @Override
        public void superOnDetach() {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onDetach();
        }

        @Override
        public void superOnDestroy() {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onDestroy();
        }

        @Override
        public void superOnSaveInstanceState(IObjectWrapper outState) {
            StrictModeWorkaround.apply();
            RemoteFragment.super.onSaveInstanceState(ObjectWrapper.unwrap(outState, Bundle.class));
        }

        @Override
        public IObjectWrapper getActivity() {
            StrictModeWorkaround.apply();
            return ObjectWrapper.wrap(RemoteFragment.this.getActivity());
        }

        @Override
        public IObjectWrapper getView() {
            StrictModeWorkaround.apply();
            return ObjectWrapper.wrap(RemoteFragment.this.getView());
        }

        @Override
        public boolean startActivityForResult(
                IObjectWrapper intent, int requestCode, IObjectWrapper options) {
            StrictModeWorkaround.apply();
            try {
                RemoteFragment.this.startActivityForResult(
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
            StrictModeWorkaround.apply();
            try {
                RemoteFragment.this.startIntentSenderForResult(
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
            StrictModeWorkaround.apply();
            return RemoteFragment.this.shouldShowRequestPermissionRationale(permission);
        }

        @Override
        public void requestPermissions(String[] permissions, int requestCode) {
            StrictModeWorkaround.apply();
            RemoteFragment.this.requestPermissions(permissions, requestCode);
        }

        @Override
        public void removeFragmentFromFragmentManager() {
            StrictModeWorkaround.apply();
            Fragment fragment = RemoteFragment.this;
            if (fragment.getParentFragmentManager() == null) return;
            fragment.getParentFragmentManager().beginTransaction().remove(fragment).commit();
        }
    };

    // Nonnull after first onAttach().
    private IRemoteFragment mRemoteFragment;

    protected RemoteFragment() {
        super();
        ThreadCheck.ensureOnUiThread();
    }

    protected abstract IRemoteFragment createRemoteFragment(Context appContext);

    protected IRemoteFragmentClient getRemoteFragmentClient() {
        return mClientImpl;
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
        if (mRemoteFragment != null) {
            return; // Already initialized.
        }
        mRemoteFragment = createRemoteFragment(appContext);
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnCreate(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        ThreadCheck.ensureOnUiThread();
        try {
            return ObjectWrapper.unwrap(
                    mRemoteFragment.handleOnCreateView(
                            ObjectWrapper.wrap(container), ObjectWrapper.wrap(savedInstanceState)),
                    View.class);
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
        try {
            mRemoteFragment.handleOnDestroy();
            // The other side does the clean up automatically in handleOnDestroy()
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
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

    protected IRemoteFragment getRemoteFragment() {
        return mRemoteFragment;
    }
}
