// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.ref.WeakReference;

/**
 * A WeakReference sublcass that's immutable.
 * This is so that it's safe to pass the same instance to multiple
 * clients without worrying about modification.
 */
@NullMarked
public class ImmutableWeakReference<T> extends WeakReference<T> {
    public ImmutableWeakReference(@Nullable T referent) {
        super(referent);
    }

    @Override
    public final void clear() {
        throw new UnsupportedOperationException("clear WeakReference banned");
    }
}
