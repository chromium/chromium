// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import java.lang.ref.WeakReference;

/**
 * A WeakReference sublcass that's immutable.
 * This is so that it's safe to pass the same instance to multiple
 * clients without worrying about modification.
 */
public class ImmutableWeakReference<T> extends WeakReference<T> {
    public ImmutableWeakReference(T referent) {
        super(referent);
    }

    @Override
    public final void clear() {
        throw new UnsupportedOperationException("clear WeakReference banned");
    }
}
