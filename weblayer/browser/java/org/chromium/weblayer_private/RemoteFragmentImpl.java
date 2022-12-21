// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Build;
import android.os.Bundle;
import android.view.SurfaceControlViewHost;
import android.view.View;

import androidx.annotation.RequiresApi;

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

    // TODO(swestphal): remove this.
    protected final Activity getActivity() {
        return null;
    }

    protected final View getView() {
        return null;
    }

    protected void onCreate() {}

    protected void onAttach(Context context) {}

    protected void onStart() {}

    protected void onDestroy() {}

    protected void onDetach() {}

    protected void onResume() {}

    protected void onDestroyView() {}

    protected void onStop() {}

    protected void onPause() {}

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {}

    protected void requestPermissions(String[] permissions, int requestCode) {}

    protected void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {}

    @RequiresApi(Build.VERSION_CODES.R)
    protected void setSurfaceControlViewHost(SurfaceControlViewHost host) {}

    protected View getContentViewRenderView() {
        return null;
    }

    protected void setMinimumSurfaceSize(int width, int height) {}

    // TODO(crbug/1378606): Either remove below methods together with callers or provide a client
    // implementation on weblayer side.
    protected boolean startActivityForResult(Intent intent, int requestCode, Bundle options) {
        return false;
    }

    protected boolean startIntentSenderForResult(IntentSender intent, int requestCode,
            Intent fillInIntent, int flagsMask, int flagsValues, int extraFlags, Bundle options) {
        return false;
    }

    protected boolean shouldShowRequestPermissionRationale(String permission) {
        return false;
    }

    protected void removeFragmentFromFragmentManager() {}

    // IRemoteFragment implementation below.

    @Override
    public final void handleOnStart() {
        StrictModeWorkaround.apply();
        onStart();
    }

    @Override
    public final void handleOnCreate() {
        StrictModeWorkaround.apply();
        onCreate();
    }

    @Override
    public final void handleOnAttach(IObjectWrapper context) {
        StrictModeWorkaround.apply();
        onAttach(ObjectWrapper.unwrap(context, Context.class));
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

    @RequiresApi(Build.VERSION_CODES.R)
    @Override
    public final void handleSetSurfaceControlViewHost(IObjectWrapper host) {
        StrictModeWorkaround.apply();
        setSurfaceControlViewHost(ObjectWrapper.unwrap(host, SurfaceControlViewHost.class));
    }

    @Override
    public final IObjectWrapper handleGetContentViewRenderView() {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(getContentViewRenderView());
    }

    @Override
    public final void handleSetMinimumSurfaceSize(int width, int height) {
        StrictModeWorkaround.apply();
        setMinimumSurfaceSize(width, height);
    }
}
