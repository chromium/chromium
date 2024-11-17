// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import static org.mockito.ArgumentMatchers.any;

import org.mockito.Mockito;
import org.mockito.stubbing.Answer;
import org.mockito.stubbing.Stubber;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;

import java.util.function.Function;

/** Simplifies common interactions with Mockito. */
public class MockitoHelper {
    /**
     * Syntactic sugar around creating an {@link Answer} to run something when a mock is invoked.
     * Not recommend for use with non-mocked / production classes. In the example below the {@link
     * Runnable} is stored in the test's member list.
     *
     * <p>doCallback((Bar bar) -> bar.baz()).when(foo).onBar(Mockito.any());
     *
     * <p>This often allows us to verify the functionality inside the provided object, trigger it
     * exactly when we want to, or verify that a sequences of events happen in the order we expect.
     * However this does not allow access to multiple arguments or to specify the return value, in
     * which case using the original doAnswer is likely better.
     *
     * <p>Note that users of this should be careful when invoking functionality directly in their
     * passed in methods. If the production code is performing operations asynchronously but the
     * tests run the same logic but synchronously, it can cause test conditions to differ.
     */
    public static <T> Stubber doCallback(Callback<T> callback) {
        return doCallback(0, callback);
    }

    /** Allows an arbitrary index for the arg that we want to call something on.  */
    public static <T> Stubber doCallback(int index, Callback<T> callback) {
        return Mockito.doAnswer(
                (Answer<Void>)
                        invocation -> {
                            T arg = (T) invocation.getArguments()[index];
                            callback.onResult(arg);
                            return null;
                        });
    }

    /** When no argument is needed. */
    public static Stubber doRunnable(Runnable runnable) {
        return Mockito.doAnswer(
                ignored -> {
                    runnable.run();
                    return null;
                });
    }

    /** Similar to {@link #doCallback(Callback)} but able to return a value as well. */
    public static <T, R> Stubber doFunction(Function<T, R> function) {
        return doFunction(function, 0);
    }

    /** Similar to {@link #doFunction(Function) but with an explicit index. */
    public static <T, R> Stubber doFunction(Function<T, R> function, int index) {
        return Mockito.doAnswer(invocation -> function.apply(invocation.getArgument(index)));
    }

    /** Forwards {@link Callback#bind} back to the callback object, allowing mocks to work. */
    public static <T> void forwardBind(Callback<T> callback) {
        Mockito.doAnswer(
                        (Answer<Runnable>)
                                invocation -> {
                                    T arg = invocation.getArgument(0);
                                    return () -> callback.onResult(arg);
                                })
                .when(callback)
                .bind(any());
    }

    /** Mockito.verify but with a timeout to reduce flakes. */
    public static <T> T waitForEvent(T mock) {
        return Mockito.verify(
                mock,
                Mockito.timeout(
                        ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }
}
