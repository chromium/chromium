// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * A wrapper service of GooglePayDataCallbacksService. The wrapping is necessary because
 * WebLayer's internal parts are not allowed to interact with the external apps directly.
 */
public class GooglePayDataCallbacksServiceWrapper extends Service {
    @Nullable
    private Service mService;

    @Override
    public void onCreate() {
        IObjectWrapper objectWrapper;
        try {
            objectWrapper = WebLayer.getIWebLayer(this).createGooglePayDataCallbacksService();
        } catch (Exception e) {
            throw new APICallException(e);
        }
        if (objectWrapper == null) return;
        Service service = ObjectWrapper.unwrap(objectWrapper, Service.class);
        if (service == null) return;
        mService = service;
        mService.onCreate();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        try {
            if (mService == null) return null;
            return mService.onBind(intent);
        } catch (Exception e) {
            throw new APICallException(e);
        }
    }
}
