// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IChildProcessService;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Delegates service calls to the chrome service implementation.
 */
@SuppressWarnings("JavadocType")
public abstract class ChildProcessService extends Service {
    private IChildProcessService mImpl;

    public ChildProcessService() {}

    @Override
    public void onCreate() {
        super.onCreate();
        try {
            WebLayer.disableWebViewCompatibilityMode();
            Context appContext = getApplicationContext();
            Context remoteContext = WebLayer.getOrCreateRemoteContext(appContext);
            mImpl = IChildProcessService.Stub.asInterface(
                    (IBinder) WebLayer
                            .loadRemoteClass(appContext,
                                    "org.chromium.weblayer_private.ChildProcessServiceImpl")
                            .getMethod("create", Service.class, Context.class, Context.class)
                            .invoke(null, this, appContext, remoteContext));
            mImpl.onCreate();
        } catch (Exception e) {
            throw new APICallException(e);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        try {
            mImpl.onDestroy();
            mImpl = null;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        try {
            return ObjectWrapper.unwrap(mImpl.onBind(ObjectWrapper.wrap(intent)), IBinder.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public static class Privileged extends ChildProcessService {}
    public static final class Privileged0 extends Privileged {}
    public static final class Privileged1 extends Privileged {}
    public static final class Privileged2 extends Privileged {}
    public static final class Privileged3 extends Privileged {}
    public static final class Privileged4 extends Privileged {}

    public static class Sandboxed extends ChildProcessService {}
    public static final class Sandboxed0 extends Sandboxed {}
    public static final class Sandboxed1 extends Sandboxed {}
    public static final class Sandboxed2 extends Sandboxed {}
    public static final class Sandboxed3 extends Sandboxed {}
    public static final class Sandboxed4 extends Sandboxed {}
    public static final class Sandboxed5 extends Sandboxed {}
    public static final class Sandboxed6 extends Sandboxed {}
    public static final class Sandboxed7 extends Sandboxed {}
    public static final class Sandboxed8 extends Sandboxed {}
    public static final class Sandboxed9 extends Sandboxed {}
    public static final class Sandboxed10 extends Sandboxed {}
    public static final class Sandboxed11 extends Sandboxed {}
    public static final class Sandboxed12 extends Sandboxed {}
    public static final class Sandboxed13 extends Sandboxed {}
    public static final class Sandboxed14 extends Sandboxed {}
    public static final class Sandboxed15 extends Sandboxed {}
    public static final class Sandboxed16 extends Sandboxed {}
    public static final class Sandboxed17 extends Sandboxed {}
    public static final class Sandboxed18 extends Sandboxed {}
    public static final class Sandboxed19 extends Sandboxed {}
    public static final class Sandboxed20 extends Sandboxed {}
    public static final class Sandboxed21 extends Sandboxed {}
    public static final class Sandboxed22 extends Sandboxed {}
    public static final class Sandboxed23 extends Sandboxed {}
    public static final class Sandboxed24 extends Sandboxed {}
    public static final class Sandboxed25 extends Sandboxed {}
    public static final class Sandboxed26 extends Sandboxed {}
    public static final class Sandboxed27 extends Sandboxed {}
    public static final class Sandboxed28 extends Sandboxed {}
    public static final class Sandboxed29 extends Sandboxed {}
    public static final class Sandboxed30 extends Sandboxed {}
    public static final class Sandboxed31 extends Sandboxed {}
    public static final class Sandboxed32 extends Sandboxed {}
    public static final class Sandboxed33 extends Sandboxed {}
    public static final class Sandboxed34 extends Sandboxed {}
    public static final class Sandboxed35 extends Sandboxed {}
    public static final class Sandboxed36 extends Sandboxed {}
    public static final class Sandboxed37 extends Sandboxed {}
    public static final class Sandboxed38 extends Sandboxed {}
    public static final class Sandboxed39 extends Sandboxed {}
}
