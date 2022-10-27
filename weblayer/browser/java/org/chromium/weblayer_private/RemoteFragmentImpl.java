// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Base for the classes controlling a Fragment that exists in another ClassLoader. Extending this
 * class is similar to extending Fragment: e.g. one can override lifecycle methods, not forgetting
 * to call super, etc.
 */
public abstract class RemoteFragmentImpl extends IRemoteFragment.Stub {
    protected RemoteFragmentImpl() {}

    @Deprecated
    public final View onCreateView() {
        return onCreateView(/*container=*/null, /*savedInstanceState=*/null);
    }

    public View onCreateView(ViewGroup container, Bundle savedInstanceState) {
        return null;
    }

    public final Activity getActivity() {
        return null;
    }

    public final View getView() {
        return null;
    }

    public void onCreate(Bundle savedInstanceState) {}

    public void onAttach(Context context) {}

    public void onActivityCreated(Bundle savedInstanceState) {}

    public void onStart() {}

    public void onDestroy() {}

    public void onDetach() {}

    public void onResume() {}

    public void onDestroyView() {}

    public void onStop() {}

    public void onPause() {}

    public void onSaveInstanceState(Bundle outState) {}

    public void onActivityResult(int requestCode, int resultCode, Intent data) {}

    public void requestPermissions(String[] permissions, int requestCode) {}

    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {}

    // TODO(crbug/1378606): Either remove below methods together with callers or provide a client
    // implementation on weblayer side.
    public boolean startActivityForResult(Intent intent, int requestCode, Bundle options) {
        return false;
    }

    public boolean startIntentSenderForResult(IntentSender intent, int requestCode,
            Intent fillInIntent, int flagsMask, int flagsValues, int extraFlags, Bundle options) {
        return false;
    }

    public boolean shouldShowRequestPermissionRationale(String permission) {
        return false;
    }

    public void removeFragmentFromFragmentManager() {}

    // IRemoteFragment implementation below.

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
