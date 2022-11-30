// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IPrerenderController;

/**
 *  Implementation of {@link IPrerenderController}.
 */
@JNINamespace("weblayer")
public class PrerenderControllerImpl extends IPrerenderController.Stub {
    private long mNativePrerenderController;
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    void destroy() {
        mNativePrerenderController = 0;

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    public PrerenderControllerImpl(long nativePrerenderController) {
        mNativePrerenderController = nativePrerenderController;
    }

    @Override
    public void prerender(String url) {
        PrerenderControllerImplJni.get().prerender(mNativePrerenderController, url);
    }

    @NativeMethods()
    interface Natives {
        void prerender(long nativePrerenderControllerImpl, String url);
    }
}
