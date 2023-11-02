// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * A base class for different types of Fragments being rendered remotely.
 *
 * This class acts as a bridge for all the of the Fragment events received from the client side and
 * the weblayer private implementation.
 */
abstract class RemoteFragmentEventHandler {
    // TODO(rayankans): Remove RemoteFragmentClients as the super events no longer need to be
    // called.
    @Nullable
    private IRemoteFragmentClientImpl mClientImpl;
    @Nullable
    private IRemoteFragment mRemoteFragment;
    @NonNull
    private Bundle mArgs;

    private static final class IRemoteFragmentClientImpl extends IRemoteFragmentClient.Stub {
        private RemoteFragmentEventHandler mRemoteFragmentEventHandler;

        IRemoteFragmentClientImpl(RemoteFragmentEventHandler remoteFragmentEventHandler) {
            mRemoteFragmentEventHandler = remoteFragmentEventHandler;
        }

        void setRemoteFragment(RemoteFragmentEventHandler remoteFragmentEventHandler) {
            mRemoteFragmentEventHandler = remoteFragmentEventHandler;
        }

        @Override
        public void superOnCreate(IObjectWrapper savedInstanceState) {}

        @Override
        public void superOnAttach(IObjectWrapper context) {}

        @Override
        public void superOnActivityCreated(IObjectWrapper savedInstanceState) {}

        @Override
        public void superOnStart() {}

        @Override
        public void superOnResume() {}

        @Override
        public void superOnPause() {}

        @Override
        public void superOnStop() {}

        @Override
        public void superOnDestroyView() {}

        @Override
        public void superOnDetach() {}

        @Override
        public void superOnDestroy() {}

        @Override
        public void superOnSaveInstanceState(IObjectWrapper outState) {}

        @Override
        public IObjectWrapper getActivity() {
            return null;
        }

        @Override
        public IObjectWrapper getView() {
            return null;
        }

        @Override
        public boolean startActivityForResult(
                IObjectWrapper intent, int requestCode, IObjectWrapper options) {
            return false;
        }

        @Override
        public boolean startIntentSenderForResult(IObjectWrapper intent, int requestCode,
                IObjectWrapper fillInIntent, int flagsMask, int flagsValues, int extraFlags,
                IObjectWrapper options) {
            return false;
        }

        @Override
        public boolean shouldShowRequestPermissionRationale(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, int requestCode) {}

        @Override
        public void removeFragmentFromFragmentManager() {}
    }

    RemoteFragmentEventHandler(Bundle args) {
        ThreadCheck.ensureOnUiThread();
        mArgs = args;
    }

    protected Bundle getArguments() {
        return mArgs;
    }

    protected abstract IRemoteFragment createRemoteFragmentEventHandler(Context appContext);

    protected IRemoteFragmentClient getRemoteFragmentClient() {
        return mClientImpl;
    }

    @CallSuper
    protected void onAttach(Context context) {
        ThreadCheck.ensureOnUiThread();
        // This is the first lifecycle event and also the first time we can get app context. So it's
        // the latest and at the same time the earliest moment when we can initialize WebLayer
        // without missing any lifecycle events.
        if (mRemoteFragment == null) {
            mClientImpl = new IRemoteFragmentClientImpl(this);
            mRemoteFragment = createRemoteFragmentEventHandler(context.getApplicationContext());
        }
        try {
            mRemoteFragment.handleOnAttach(ObjectWrapper.wrap(context));
            // handleOnAttach results in creating BrowserImpl on the other side.
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onCreate(Bundle savedInstanceState) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnCreate(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onStart() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnStart();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onResume() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnResume();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onSaveInstanceState(Bundle outState) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnSaveInstanceState(ObjectWrapper.wrap(outState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onPause() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnPause();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onStop() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnStop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onDestroy() {
        ThreadCheck.ensureOnUiThread();

        try {
            mRemoteFragment.handleOnDestroy();
            // The other side does the clean up automatically in handleOnDestroy()
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onDetach() {
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
