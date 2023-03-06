// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import org.mockito.Mockito;
import org.mockito.stubbing.Answer;
import org.mockito.stubbing.Stubber;

import org.chromium.base.Callback;

/** Simplifies common interactions with Mockito. */
public class MockitoHelper {
    /**
     * Syntactic sugar around creating an {@link Answer} to run something when a mock is invoked.
     * Not recommend for use with non-mocked / production classes. In the example below the
     * {@link Runnable} is stored in the test's member list.
     *
     * doCallback((Bar bar) -> bar.baz()).when(foo).onBar(Mockito.any());
     *
     * This often allows us to verify the functionality inside the provided object, trigger it
     * exactly when we want to, or verify that a sequences of events happen in the order we expect.
     * However this does not allow access to multiple arguments or to specify the return value, in
     * which case using the original doAnswer is likely better.
     *
     * Note that users of this should be careful when invoking functionality directly in their
     * passed in methods. If the production code is performing operations asynchronously but the
     * tests run the same logic but synchronously, it can cause test conditions to differ.
     */
    public static <T> Stubber doCallback(Callback<T> callback) {
        return doCallback(0, callback);
    }

    /** Allows an arbitrary index for the arg that we want to call something on.  */
    public static <T> Stubber doCallback(int index, Callback<T> callback) {
        return Mockito.doAnswer((Answer<Void>) invocation -> {
            T arg = (T) invocation.getArguments()[index];
            callback.onResult(arg);
            return null;
        });
    }

    /** When no argument is needed. */
    public static <T> Stubber doRunnable(Runnable runnable) {
        return Mockito.doAnswer((ignored -> {
            runnable.run();
            return null;
        }));
    }
}