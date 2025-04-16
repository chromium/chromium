/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

package org.tensorflow.lite.support.model;

import java.io.Closeable;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.checkerframework.checker.nullness.qual.Nullable;
import org.tensorflow.lite.Delegate;
import org.tensorflow.lite.InterpreterApi;
import org.tensorflow.lite.InterpreterApi.Options.TfLiteRuntime;

/**
 * Helper class to create and call necessary methods of {@code GpuDelegate} which is not a strict
 * dependency.
 */
class GpuDelegateProxy implements Delegate, Closeable {

  private static final Logger logger = Logger.getLogger(InterpreterApi.class.getName());

  private final Delegate proxiedDelegate;
  private final Closeable proxiedCloseable;

  // We log at most once for each different TfLiteRuntime value.
  private static final AtomicBoolean[] haveLogged =
      new AtomicBoolean[TfLiteRuntime.values().length];

  static {
    for (int i = 0; i < TfLiteRuntime.values().length; i++) {
      haveLogged[i] = new AtomicBoolean();
    }
  }

  @Nullable
  public static GpuDelegateProxy maybeNewInstance(TfLiteRuntime runtime) {
    Exception exception = null;
    if (runtime == null) {
      runtime = TfLiteRuntime.FROM_APPLICATION_ONLY;
    }
    if (runtime == TfLiteRuntime.PREFER_SYSTEM_OVER_APPLICATION
        || runtime == TfLiteRuntime.FROM_SYSTEM_ONLY) {
      try {
        Class<?> clazz = Class.forName("com.google.android.gms.tflite.gpu.GpuDelegate");
        Object instance = clazz.getDeclaredConstructor().newInstance();
        if (!haveLogged[runtime.ordinal()].getAndSet(true)) {
          logger.info(
              String.format(
                  "TfLiteRuntime.%s: "
                      + "Using system GpuDelegate from com.google.android.gms.tflite.gpu",
                  runtime.name()));
        }
        return new GpuDelegateProxy(instance);
      } catch (ReflectiveOperationException e) {
        exception = e;
      }
    }
    if (runtime == TfLiteRuntime.PREFER_SYSTEM_OVER_APPLICATION
        || runtime == TfLiteRuntime.FROM_APPLICATION_ONLY) {
      try {
        Class<?> clazz = Class.forName("org.tensorflow.lite.gpu.GpuDelegate");
        Object instance = clazz.getDeclaredConstructor().newInstance();
        if (!haveLogged[runtime.ordinal()].getAndSet(true)) {
          logger.info(
              String.format(
                  "TfLiteRuntime.%s: "
                      + "Using application GpuDelegate from org.tensorflow.lite.gpu",
                  runtime.name()));
        }
        return new GpuDelegateProxy(instance);
      } catch (ReflectiveOperationException e) {
        if (exception == null) {
          exception = e;
        } else if (exception.getSuppressed().length == 0) {
          exception.addSuppressed(e);
        }
      }
    }
    if (!haveLogged[runtime.ordinal()].getAndSet(true)) {
      String className = "org.tensorflow.lite.support.model.Model.Options.Builder";
      String methodName = "setTfLiteRuntime";
      String message;
      switch (runtime) {
        case FROM_APPLICATION_ONLY:
          message =
              String.format(
                  "You should declare a build dependency on"
                      + " org.tensorflow.lite:tensorflow-lite-gpu,"
                      + " or call .%s with a value other than TfLiteRuntime.FROM_APPLICATION_ONLY"
                      + " (see docs for %s#%s(TfLiteRuntime)).",
                  methodName, className, methodName);
          break;
        case FROM_SYSTEM_ONLY:
          message =
              String.format(
                  "You should declare a build dependency on"
                      + " com.google.android.gms:play-services-tflite-gpu,"
                      + " or call .%s with a value other than TfLiteRuntime.FROM_SYSTEM_ONLY "
                      + " (see docs for %s#%s).",
                  methodName, className, methodName);
          break;
        default:
          message =
              "You should declare a build dependency on"
                  + " org.tensorflow.lite:tensorflow-lite-gpu or"
                  + " com.google.android.gms:play-services-tflite-gpu";
          break;
      }
      logger.log(
          Level.WARNING,
          "Couldn't find TensorFlow Lite's GpuDelegate class --"
              + " make sure your app links in the right GPU delegate. "
              + message,
          exception);
    }
    return null;
  }

  /** Calls {@code close()} method of the delegate. */
  @Override
  public void close() {
    try {
      proxiedCloseable.close();
    } catch (IOException e) {
      // Should not trigger, because GpuDelegate#close never throws. The catch is required because
      // of Closeable#close.
      logger.log(Level.SEVERE, "Failed to close the GpuDelegate.", e);
    }
  }

  /** Calls {@code getNativeHandle()} method of the delegate. */
  @Override
  public long getNativeHandle() {
    return proxiedDelegate.getNativeHandle();
  }

  private GpuDelegateProxy(Object instance) {
    this.proxiedCloseable = (Closeable) instance;
    this.proxiedDelegate = (Delegate) instance;
  }
}
