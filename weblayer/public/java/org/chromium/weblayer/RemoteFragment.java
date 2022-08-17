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

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModel;

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
    // RemoteFragment provides basic support for saving state to a ViewModel. This enables the
    // Browser to persist during configuration changes, when the system destroys the
    // BrowserFragment and creates a new one. From the implementation sides perspective, it sees the
    // same set of events as occurs with setRetainInstance(true). While the RemoteFragment is
    // recreated during a configuration change, the IRemoteFragmentClient supplied to the remote
    // side does not change. RemoteFragment saves the IRemoteFragmentClientImpl to a ViewModel. If a
    // new RemoteFragment is created it takes the IRemoteFragmentClientImpl from the ViewModel and
    // changes the RemoteFragment the IRemoteFragmentClientImpl uses. This way the implementation
    // side never knows about the change, it's purely on the client side.
    //
    // To support persisting state to a ViewModel a subclass must do the following:
    // . Create a subclass of ViewModelImpl to add the necessary state.
    // . override onAttach() if there is existing state to use, extract it and
    //   call super.configureFromViewModel().
    // . Ensure state is not created twice. If the subclass creates state in onCreate(), it will
    //   likely need to ensure that doesn't happen if state was extracted in onAttach().
    // . Ensure state is not destroyed in onDestroy(). Instead, state should be destroyed in the
    //   ViewModelImpl subclass.
    // . Override onCreate() and call saveToViewModel().
    //
    // As the ordering of these is subtle, and the ViewModel implementation needs to be configured
    // by the subclass, RemoteFragment doesn't attempt to automate this.

    @Nullable
    private IRemoteFragmentClientImpl mClientImpl;
    @Nullable
    private IRemoteFragment mRemoteFragment;
    // Whether configureFromViewModel() was called. In other words, state was restored from
    // ViewModelImpl.
    private boolean mConfiguredFromViewModelState;

    // Whetner saveToViewModel() was called. In other words, state was saved to ViewModelImpl.
    private boolean mSavedStateToViewModel;

    // Whether to forward the onCreate and onDestroy events to the remote fragment.
    // TODO(rayankans): Remove this hack once BrowserFragmentDelegate is merged with
    // BrowserFragment.
    protected boolean mForwardCreateDestroyEvents = true;

    private static final class IRemoteFragmentClientImpl extends IRemoteFragmentClient.Stub {
        private RemoteFragment mRemoteFragment;
        // Used to track when destroy is being called because a ViewModel is being destroyed. When
        // this happens destroy should called on the remote side only (not to RemoteFragment.super).
        private boolean mInViewModelDestroy;

        IRemoteFragmentClientImpl(RemoteFragment remoteFragment) {
            mRemoteFragment = remoteFragment;
        }

        void setRemoteFragment(RemoteFragment remoteFragment) {
            mRemoteFragment = remoteFragment;
        }

        public void callDestroyFromViewModel() {
            StrictModeWorkaround.apply();
            mInViewModelDestroy = true;
            try {
                mRemoteFragment.mRemoteFragment.handleOnDestroy();
            } catch (RemoteException e) {
                throw new APICallException(e);
            } finally {
                mInViewModelDestroy = false;
            }
        }

        @Override
        public void superOnCreate(IObjectWrapper savedInstanceState) {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnAttach(IObjectWrapper context) {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnAttach(ObjectWrapper.unwrap(context, Context.class));
        }

        @Override
        public void superOnActivityCreated(IObjectWrapper savedInstanceState) {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnStart() {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnStart();
        }

        @Override
        public void superOnResume() {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnResume();
        }

        @Override
        public void superOnPause() {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnPause();
        }

        @Override
        public void superOnStop() {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnStop();
        }

        @Override
        public void superOnDestroyView() {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnDestroyView();
        }

        @Override
        public void superOnDetach() {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnDetach();
        }

        @Override
        public void superOnDestroy() {
            StrictModeWorkaround.apply();
            if (!mInViewModelDestroy) {
                mRemoteFragment.superOnDestroy();
            }
        }

        @Override
        public void superOnSaveInstanceState(IObjectWrapper outState) {
            StrictModeWorkaround.apply();
            mRemoteFragment.superOnSaveInstanceState(ObjectWrapper.unwrap(outState, Bundle.class));
        }

        @Override
        public IObjectWrapper getActivity() {
            StrictModeWorkaround.apply();
            return ObjectWrapper.wrap(mRemoteFragment.getActivity());
        }

        @Override
        public IObjectWrapper getView() {
            StrictModeWorkaround.apply();
            return ObjectWrapper.wrap(mRemoteFragment.getView());
        }

        @Override
        public boolean startActivityForResult(
                IObjectWrapper intent, int requestCode, IObjectWrapper options) {
            StrictModeWorkaround.apply();
            try {
                mRemoteFragment.startActivityForResult(ObjectWrapper.unwrap(intent, Intent.class),
                        requestCode, ObjectWrapper.unwrap(options, Bundle.class));
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
                mRemoteFragment.startIntentSenderForResult(
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
            return mRemoteFragment.shouldShowRequestPermissionRationale(permission);
        }

        @Override
        public void requestPermissions(String[] permissions, int requestCode) {
            StrictModeWorkaround.apply();
            mRemoteFragment.requestPermissions(permissions, requestCode);
        }

        @Override
        public void removeFragmentFromFragmentManager() {
            StrictModeWorkaround.apply();
            if (mRemoteFragment.getParentFragmentManager() == null) return;
            mRemoteFragment.getParentFragmentManager()
                    .beginTransaction()
                    .remove(mRemoteFragment)
                    .commit();
        }
    }

    /**
     * RemoteFragment implementation of ViewModel. See earlier comments for details on this.
     */
    static class ViewModelImpl extends ViewModel {
        @Nullable
        private IRemoteFragmentClientImpl mClientImpl;
        @Nullable
        private IRemoteFragment mRemoteFragment;

        public ViewModelImpl() {}

        @Override
        @CallSuper
        protected void onCleared() {
            ThreadCheck.ensureOnUiThread();
            if (mClientImpl != null) {
                mClientImpl.callDestroyFromViewModel();
                mClientImpl = null;
                mRemoteFragment = null;
            }
            super.onCleared();
        }
    }

    protected RemoteFragment() {
        super();
        ThreadCheck.ensureOnUiThread();
    }

    protected abstract IRemoteFragment createRemoteFragment(Context appContext);

    protected IRemoteFragmentClient getRemoteFragmentClient() {
        return mClientImpl;
    }

    /**
     * Configures this Fragment from state saved to a ViewModel. This should be called from the
     * subclasses implementation of onAttach().
     */
    void configureFromViewModel(@NonNull ViewModelImpl model) {
        // This should be called at one of two times: before initial attach, subsequent attach with
        // same state.
        assert mClientImpl == null || model.mClientImpl == mClientImpl;
        mClientImpl = model.mClientImpl;
        mClientImpl.setRemoteFragment(this);
        mRemoteFragment = model.mRemoteFragment;
        mConfiguredFromViewModelState = true;
    }

    /**
     * Saves the Fragment state to a ViewModel. This should generally be called from onCreate().
     */
    void saveToViewModel(@NonNull ViewModelImpl model) {
        // If this is called before onCreateView(), the cached state won't be valid.
        assert mRemoteFragment != null;
        assert mClientImpl != null;
        mSavedStateToViewModel = true;
        model.mClientImpl = mClientImpl;
        model.mRemoteFragment = mRemoteFragment;
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
        // This is the first lifecycle event and also the first time we can get app context. So it's
        // the latest and at the same time the earliest moment when we can initialize WebLayer
        // without missing any lifecycle events.
        if (mRemoteFragment == null) {
            mClientImpl = new IRemoteFragmentClientImpl(this);
            mRemoteFragment = createRemoteFragment(context.getApplicationContext());
        }
        try {
            mRemoteFragment.handleOnAttach(ObjectWrapper.wrap(context));
            // handleOnAttach results in creating BrowserImpl on the other side.
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        ThreadCheck.ensureOnUiThread();
        // onCreate() should only be forwarded to the remote side once (this matches what
        // happens with retained fragments).
        if (mConfiguredFromViewModelState) {
            super.onCreate(savedInstanceState);
            return;
        }
        if (!mForwardCreateDestroyEvents) {
            return;
        }
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
        if (mSavedStateToViewModel || mConfiguredFromViewModelState) {
            // When the state is saved to a ViewModel the ViewModel handles destruction, not the
            // RemoteFragment.
            super.onDestroy();
            return;
        }
        ThreadCheck.ensureOnUiThread();

        if (!mForwardCreateDestroyEvents) {
            return;
        }

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

    private void superOnCreate(Bundle savedInstanceState) {
        // TODO(rayankans): Remove calls into the base Fragment class.
        try {
            super.onCreate(savedInstanceState);
        } catch (NullPointerException e) {
        }
    }

    private void superOnAttach(Context context) {
        super.onAttach(context);
    }

    private void superOnStart() {
        super.onStart();
    }

    private void superOnResume() {
        super.onResume();
    }

    private void superOnPause() {
        super.onPause();
    }

    private void superOnStop() {
        super.onStop();
    }

    private void superOnDestroyView() {
        super.onDestroyView();
    }

    private void superOnDetach() {
        super.onDetach();
    }

    private void superOnDestroy() {
        super.onDestroy();
    }

    private void superOnSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
    }
}
