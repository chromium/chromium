// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import static org.junit.Assert.assertNotNull;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.ui.resources.Resource;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Test utils class to share accessor patterns for working with {link @DynamicResoursce}. Primarily
 * for old tests that were written and verify the behavior of synchronous resources.
 */
public final class DynamicResourceTestUtils {
    /** Only works on {@link DynamicResource} that synchronously invoke their callback. */
    public static Rect getBitmapSizeSync(DynamicResource dynamicResource) {
        return getResourceSync(dynamicResource).getBitmapSize();
    }

    /** Only works on {@link DynamicResource} that synchronously invoke their callback. */
    public static Bitmap getBitmapSync(DynamicResource dynamicResource) {
        return getResourceSync(dynamicResource).getBitmap();
    }

    /** Only works on {@link DynamicResource} that synchronously invoke their callback. */
    public static Resource getResourceSync(DynamicResource dynamicResource) {
        AtomicReference<Resource> resourcePointer = new AtomicReference<>();
        Callback<Resource> callback = (resource) -> resourcePointer.set(resource);
        dynamicResource.addOnResourceReadyCallback(callback);
        dynamicResource.onResourceRequested();
        Resource temp = resourcePointer.get();
        assertNotNull(temp);

        // Clear out the callback which is owning the AtomicReference, which in turn keeps alive the
        // Resource and/or Bitmap. Some tests will verify GC is able to reclaim these.
        dynamicResource.removeOnResourceReadyCallback(callback);

        return temp;
    }
}
