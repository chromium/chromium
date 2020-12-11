// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.process_launcher.ChildProcessService;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.app.ChildProcessServiceFactory;
import org.chromium.weblayer_private.interfaces.IChildProcessService;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Implementation of IChildProcessService.
 */
@UsedByReflection("WebLayer")
public final class ChildProcessServiceImpl extends IChildProcessService.Stub {
    private ChildProcessService mService;

    @UsedByReflection("WebLayer")
    public static IBinder create(Service service, Context appContext, Context remoteContext) {
        ClassLoaderContextWrapperFactory.setLightDarkResourceOverrideContext(
                remoteContext, remoteContext);
        // Wrap the app context so that it can be used to load WebLayer implementation classes.
        appContext = ClassLoaderContextWrapperFactory.get(appContext);
        return new ChildProcessServiceImpl(service, appContext);
    }

    @Override
    public void onCreate() {
        StrictModeWorkaround.apply();
        mService.onCreate();
    }

    @Override
    public void onDestroy() {
        StrictModeWorkaround.apply();
        mService.onDestroy();
        mService = null;
    }

    @Override
    public IObjectWrapper onBind(IObjectWrapper intent) {
        StrictModeWorkaround.apply();
        return ObjectWrapper.wrap(mService.onBind(ObjectWrapper.unwrap(intent, Intent.class)));
    }

    private ChildProcessServiceImpl(Service service, Context context) {
        mService = ChildProcessServiceFactory.create(service, context);
    }
}
