// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.RemoteException;
import android.view.SurfaceControlViewHost;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

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
    @Nullable
    private IRemoteFragment mRemoteFragment;

    private RemoteFragmentClientImpl mRemoteFragmentClient;

    RemoteFragmentEventHandler(IRemoteFragment remoteFragment) {
        ThreadCheck.ensureOnUiThread();
        mRemoteFragment = remoteFragment;

        mRemoteFragmentClient = new RemoteFragmentClientImpl();
        try {
            mRemoteFragment.setClient(mRemoteFragmentClient);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onAttach(Context context, @Nullable Fragment fragment) {
        ThreadCheck.ensureOnUiThread();
        mRemoteFragmentClient.setFragment(fragment);
        try {
            mRemoteFragment.handleOnAttach(ObjectWrapper.wrap(context));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onCreate() {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnCreate();
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
        mRemoteFragmentClient.setFragment(null);
        try {
            mRemoteFragment.handleOnDetach();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleOnActivityResult(
                    requestCode, resultCode, ObjectWrapper.wrap(intent));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected View getContentViewRenderView() {
        ThreadCheck.ensureOnUiThread();
        try {
            return ObjectWrapper.unwrap(
                    mRemoteFragment.handleGetContentViewRenderView(), View.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void setSurfaceControlViewHost(SurfaceControlViewHost host) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleSetSurfaceControlViewHost(ObjectWrapper.wrap(host));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CallSuper
    protected void setMinimumSurfaceSize(int width, int height) {
        ThreadCheck.ensureOnUiThread();
        try {
            mRemoteFragment.handleSetMinimumSurfaceSize(width, height);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    protected IRemoteFragment getRemoteFragment() {
        return mRemoteFragment;
    }

    final class RemoteFragmentClientImpl extends IRemoteFragmentClient.Stub {
        // The WebFragment. Only available for in-process mode.
        @Nullable
        private Fragment mFragment;

        void setFragment(@Nullable Fragment fragment) {
            mFragment = fragment;
        }

        @Override
        public boolean startActivityForResult(
                IObjectWrapper intent, int requestCode, IObjectWrapper options) {
            if (mFragment != null) {
                mFragment.startActivityForResult(ObjectWrapper.unwrap(intent, Intent.class),
                        requestCode, ObjectWrapper.unwrap(options, Bundle.class));
                return true;
            }
            return false;
        }
    }
}
