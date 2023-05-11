// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.webengine.interfaces.IProfileManagerDelegate;
import org.chromium.webengine.interfaces.IStringListCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * This class acts as a proxy between the WebSandbox and Profiles.
 */
public class ProfileManagerDelegate extends IProfileManagerDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private WebLayer mWeblayer;

    ProfileManagerDelegate(WebLayer webLayer) {
        mWeblayer = webLayer;
    }

    @Override
    public void getAllProfileNames(IStringListCallback callback) {
        mHandler.post(() -> {
            try {
                List<String> names = new ArrayList<>();
                for (Profile profile : Profile.getAllProfiles()) {
                    names.add(profile.getName());
                }
                callback.onResult(names);
            } catch (RemoteException e) {
            }
        });
    }
}
