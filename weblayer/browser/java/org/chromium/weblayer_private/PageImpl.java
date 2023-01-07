// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IClientPage;

/**
 * Implementation side of Page.
 */
@JNINamespace("weblayer")
public final class PageImpl {
    // Will be null for clients libraries that are old and don't support this.
    private final IClientPage mClientPage;
    // WARNING: PageImpl may outlive the native side, in which case this member is set to 0.
    private long mNativePageImpl;
    private final NavigationControllerImpl mNavigationController;

    public PageImpl(IClientPage clientPage, long nativePageImpl,
            NavigationControllerImpl navigationController) {
        mClientPage = clientPage;
        mNativePageImpl = nativePageImpl;
        mNavigationController = navigationController;
        PageImplJni.get().setJavaPage(mNativePageImpl, PageImpl.this);
    }

    public IClientPage getClientPage() {
        return mClientPage;
    }

    long getNativePageImpl() {
        return mNativePageImpl;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNavigationController.onPageDestroyed(this);
        mNativePageImpl = 0;
    }

    @NativeMethods
    interface Natives {
        void setJavaPage(long nativePageImpl, PageImpl caller);
    }
}
