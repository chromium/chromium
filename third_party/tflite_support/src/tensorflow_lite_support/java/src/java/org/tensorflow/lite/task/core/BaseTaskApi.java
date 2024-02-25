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

package org.tensorflow.lite.task.core;

import android.util.Log;
import java.io.Closeable;

/**
 * Base class for Task API, provides shared logic to load/unload native libs to its C++ counterpart.
 */
public abstract class BaseTaskApi implements Closeable {
  private static final String TAG = BaseTaskApi.class.getSimpleName();

  /**
   * Represents a pointer to the corresponding C++ task_api object. The nativeHandle pointer is
   * initialized from subclasses and must be released by calling {@link #deinit} after it is no
   * longer needed.
   */
  private final long nativeHandle;

  /** Indicates whether the {@link #nativeHandle} pointer has been released yet. */
  private boolean closed;

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++.
   */
  protected BaseTaskApi(long nativeHandle) {
    if (nativeHandle == TaskJniUtils.INVALID_POINTER) {
      throw new IllegalArgumentException("Failed to load C++ pointer from JNI");
    }
    this.nativeHandle = nativeHandle;
  }

  public boolean isClosed() {
    return closed;
  }

  /** Release the memory allocated from C++ and deregister the library from the static holder. */
  @Override
  public synchronized void close() {
    if (closed) {
      return;
    }
    deinit(nativeHandle);
    closed = true;
  }

  public long getNativeHandle() {
    return nativeHandle;
  }

  protected void checkNotClosed() {
    if (isClosed()) {
      throw new IllegalStateException("Internal error: The task lib has already been closed.");
    }
  }

  @Override
  protected void finalize() throws Throwable {
    try {
      if (!closed) {
        Log.w(TAG, "Closing an already closed native lib");
        close();
      }
    } finally {
      super.finalize();
    }
  }

  /**
   * Releases memory pointed by the pointer in the native layer.
   *
   * @param nativeHandle pointer to memory allocated
   */
  protected abstract void deinit(long nativeHandle);
}
