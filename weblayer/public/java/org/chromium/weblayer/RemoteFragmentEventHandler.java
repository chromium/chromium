// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.RemoteException;
import android.view.SurfaceControlViewHost;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
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

    RemoteFragmentEventHandler(Browser browser) {
        ThreadCheck.ensureOnUiThread();
        mRemoteFragment = createRemoteFragmentEventHandler(browser);
    }

    protected abstract IRemoteFragment createRemoteFragmentEventHandler(Browser browser);

    @CallSuper
    protected void onAttach(Context context) {
        ThreadCheck.ensureOnUiThread();

        try {
            mRemoteFragment.handleOnAttach(ObjectWrapper.wrap(context));
            // handleOnAttach results in creating BrowserImpl on the other side.
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
        try {
            mRemoteFragment.handleOnDetach();
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
}
