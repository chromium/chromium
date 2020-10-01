// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.os.RemoteException;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Base for the classes controlling a Fragment that exists in another ClassLoader. Extending this
 * class is similar to extending Fragment: e.g. one can override lifecycle methods, not forgetting
 * to call super, etc.
 */
public abstract class RemoteFragmentImpl extends IRemoteFragment.Stub {
    private final IRemoteFragmentClient mClient;

    protected RemoteFragmentImpl(IRemoteFragmentClient client) {
        mClient = client;
    }

    @Deprecated
    public final View onCreateView() {
        return onCreateView(/*container=*/null, /*savedInstanceState=*/null);
    }

    public View onCreateView(ViewGroup container, Bundle savedInstanceState) {
        return null;
    }

    public final Activity getActivity() {
        try {
            return ObjectWrapper.unwrap(mClient.getActivity(), Activity.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public final View getView() {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 84) {
            return null;
        }
        try {
            return ObjectWrapper.unwrap(mClient.getView(), View.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void removeFragmentFromFragmentManager() {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 87) {
            return;
        }
        try {
            mClient.removeFragmentFromFragmentManager();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    // TODO(pshmakov): add dependency to androidx.annotation and put @CallSuper here.
    public void onCreate(Bundle savedInstanceState) {
        try {
            mClient.superOnCreate(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onAttach(Context context) {
        try {
            mClient.superOnAttach(ObjectWrapper.wrap(context));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onActivityCreated(Bundle savedInstanceState) {
        try {
            mClient.superOnActivityCreated(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {}

    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {}

    public void onStart() {
        try {
            mClient.superOnStart();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onDestroy() {
        try {
            mClient.superOnDestroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onDetach() {
        try {
            mClient.superOnDetach();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onResume() {
        try {
            mClient.superOnResume();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onDestroyView() {
        try {
            mClient.superOnDestroyView();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onStop() {
        try {
            mClient.superOnStop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onPause() {
        try {
            mClient.superOnPause();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void onSaveInstanceState(Bundle outState) {
        try {
            mClient.superOnSaveInstanceState(ObjectWrapper.wrap(outState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public boolean startActivityForResult(Intent intent, int requestCode, Bundle options) {
        try {
            return mClient.startActivityForResult(
                    ObjectWrapper.wrap(intent), requestCode, ObjectWrapper.wrap(options));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public boolean startIntentSenderForResult(IntentSender intent, int requestCode,
            Intent fillInIntent, int flagsMask, int flagsValues, int extraFlags, Bundle options) {
        try {
            return mClient.startIntentSenderForResult(ObjectWrapper.wrap(intent), requestCode,
                    ObjectWrapper.wrap(fillInIntent), flagsMask, flagsValues, extraFlags,
                    ObjectWrapper.wrap(options));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public boolean shouldShowRequestPermissionRationale(String permission) {
        try {
            return mClient.shouldShowRequestPermissionRationale(permission);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void requestPermissions(String[] permissions, int requestCode) {
        try {
            mClient.requestPermissions(permissions, requestCode);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    // IRemoteFragment implementation below.

    @Override
    @Deprecated
    public final IObjectWrapper deprecatedHandleOnCreateView() {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(onCreateView());
    }

    @Override
    public final IObjectWrapper handleOnCreateView(
            IObjectWrapper container, IObjectWrapper savedInstanceState) {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(onCreateView(ObjectWrapper.unwrap(container, ViewGroup.class),
                ObjectWrapper.unwrap(savedInstanceState, Bundle.class)));
    }

    @Override
    public final void handleOnStart() {
        StrictModeWorkaround.apply();
        onStart();
    }

    @Override
    public final void handleOnCreate(IObjectWrapper savedInstanceState) {
        StrictModeWorkaround.apply();
        onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
    }

    @Override
    public final void handleOnAttach(IObjectWrapper context) {
        StrictModeWorkaround.apply();
        onAttach(ObjectWrapper.unwrap(context, Context.class));
    }

    @Override
    public final void handleOnActivityCreated(IObjectWrapper savedInstanceState) {
        StrictModeWorkaround.apply();
        onActivityCreated(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
    }

    @Override
    public final void handleOnResume() {
        StrictModeWorkaround.apply();
        onResume();
    }

    @Override
    public final void handleOnPause() {
        StrictModeWorkaround.apply();
        onPause();
    }

    @Override
    public final void handleOnStop() {
        StrictModeWorkaround.apply();
        onStop();
    }

    @Override
    public final void handleOnDestroyView() {
        StrictModeWorkaround.apply();
        onDestroyView();
    }

    @Override
    public final void handleOnDetach() {
        StrictModeWorkaround.apply();
        onDetach();
    }

    @Override
    public final void handleOnDestroy() {
        StrictModeWorkaround.apply();
        onDestroy();
    }

    @Override
    public final void handleOnSaveInstanceState(IObjectWrapper outState) {
        StrictModeWorkaround.apply();
        onSaveInstanceState(ObjectWrapper.unwrap(outState, Bundle.class));
    }

    @Override
    public final void handleOnActivityResult(int requestCode, int resultCode, IObjectWrapper data) {
        StrictModeWorkaround.apply();
        onActivityResult(requestCode, resultCode, ObjectWrapper.unwrap(data, Intent.class));
    }

    @Override
    public final void handleOnRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        StrictModeWorkaround.apply();
        onRequestPermissionsResult(requestCode, permissions, grantResults);
    }
}
