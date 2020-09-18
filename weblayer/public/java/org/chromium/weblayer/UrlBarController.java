// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IUrlBarController;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * UrlBarController enables creation of URL bar views and retrieval of information about them.
 */
public class UrlBarController {
    private final IUrlBarController mImpl;

    // Constructor for test mocking.
    protected UrlBarController() {
        mImpl = null;
    }

    UrlBarController(IUrlBarController urlBarController) {
        mImpl = urlBarController;
    }

    /**
     * Creates a URL bar view based on the options provided.
     * @param options The options provided to tweak the URL bar display.
     * @since 82
     */
    @NonNull
    public View createUrlBarView(@NonNull UrlBarOptions options) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (WebLayer.getSupportedMajorVersionInternal() < 86) {
                return ObjectWrapper.unwrap(
                        mImpl.deprecatedCreateUrlBarView(options.getBundle()), View.class);
            }
            return ObjectWrapper.unwrap(
                    mImpl.createUrlBarView(options.getBundle(),
                            ObjectWrapper.wrap(options.getTextClickListener()),
                            ObjectWrapper.wrap(options.getTextLongClickListener())),
                    View.class);
        } catch (RemoteException exception) {
            throw new APICallException(exception);
        }
    }
}
