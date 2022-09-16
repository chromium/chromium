// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * A client-side service that wraps the impl-side PaymentDetailsUpdateService. WebLayer embedders
 * export this service via //weblayer/public/java/AndroidManifest.xml so external payment apps can
 * notify WebLayer of any updates to the selected payment details.
 *
 * @since 92
 */
public class PaymentDetailsUpdateServiceWrapper extends Service {
    /** The impl-side of PaymentDetailsUpdateService. Can be null if WebLayer wasn't loaded yet. */
    @Nullable
    private Service mService;

    @Override
    public void onCreate() {
        ThreadCheck.ensureOnUiThread();
        Service service = createService();
        if (service == null) {
            stopSelf();
            return;
        }
        mService = service;
        mService.onCreate();
    }

    @Nullable
    private Service createService() {
        if (WebLayer.getSupportedMajorVersionInternal() < 92) {
            throw new UnsupportedOperationException();
        }

        // WebLayer started the calling app so we assume WebLayer is still running. This service is
        // not supposed to be started when WebLayer isn't running so we can safely ignore requests
        // in that case.
        if (!WebLayer.hasWebLayerInitializationStarted()) return null;
        WebLayer webLayer = WebLayer.getLoadedWebLayer(this);
        if (webLayer == null) return null;
        IWebLayer iWebLayer = webLayer.getImpl();
        if (iWebLayer == null) return null;

        IObjectWrapper objectWrapper;
        try {
            objectWrapper = iWebLayer.createPaymentDetailsUpdateService();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        if (objectWrapper == null) return null;
        return ObjectWrapper.unwrap(objectWrapper, Service.class);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        if (mService == null) return null;
        try {
            return mService.onBind(intent);
        } catch (Exception e) {
            throw new APICallException(e);
        }
    }
}
