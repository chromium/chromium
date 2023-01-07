// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * A service used internally by WebLayer for decoding images on the local device.
 */
class ImageDecoderService extends Service {
    private IBinder mImageDecoder;

    @Override
    public void onCreate() {
        try {
            mImageDecoder =
                    WebLayer.getIWebLayer(this).initializeImageDecoder(ObjectWrapper.wrap(this),
                            ObjectWrapper.wrap(WebLayer.getOrCreateRemoteContext(this)));
        } catch (Exception e) {
            throw new APICallException(e);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mImageDecoder;
    }
}
