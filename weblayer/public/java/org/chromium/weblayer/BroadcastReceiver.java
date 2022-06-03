// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.Intent;
import android.os.RemoteException;

import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Listens to events from WebLayer-spawned notifications.
 */
public class BroadcastReceiver extends android.content.BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        try {
            WebLayer.loadAsync(context, webLayer -> {
                try {
                    webLayer.getImpl().onReceivedBroadcast(ObjectWrapper.wrap(context), intent);
                } catch (RemoteException e) {
                    throw new RuntimeException(e);
                }
            });
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException(e);
        }
    }
}
