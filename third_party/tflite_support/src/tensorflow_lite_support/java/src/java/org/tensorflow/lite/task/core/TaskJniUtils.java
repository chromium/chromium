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

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.util.Log;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;

/** JNI utils for Task API. */
public class TaskJniUtils {
  public static final long INVALID_POINTER = 0;
  private static final String TAG = TaskJniUtils.class.getSimpleName();
  /** Syntax sugar to get nativeHandle from empty param list. */
  public interface EmptyHandleProvider {
    long createHandle();
  }

  /** Syntax sugar to get nativeHandle from an array of {@link ByteBuffer}s. */
  public interface MultipleBuffersHandleProvider {
    long createHandle(ByteBuffer... buffers);
  }

  /** Syntax sugar to get nativeHandle from file descriptor and options. */
  public interface FdAndOptionsHandleProvider<T> {
    long createHandle(
        int fileDescriptor, long fileDescriptorLength, long fileDescriptorOffset, T options);
  }

  /**
   * Initializes the JNI and returns C++ handle with file descriptor and options for task API.
   *
   * @param context the Android app context
   * @param provider provider to get C++ handle, usually returned from native call
   * @param libName name of C++ lib to be loaded
   * @param filePath path of the file to be loaded
   * @param options options to set up the task API, used by the provider
   * @return C++ handle as long
   * @throws IOException If model file fails to load.
   */
  public static <T> long createHandleFromFdAndOptions(
      Context context,
      final FdAndOptionsHandleProvider<T> provider,
      String libName,
      String filePath,
      final T options)
      throws IOException {
    try (AssetFileDescriptor assetFileDescriptor = context.getAssets().openFd(filePath)) {
      return createHandleFromLibrary(
          new EmptyHandleProvider() {
            @Override
            public long createHandle() {
              return provider.createHandle(
                  /*fileDescriptor=*/ assetFileDescriptor.getParcelFileDescriptor().getFd(),
                  /*fileDescriptorLength=*/ assetFileDescriptor.getLength(),
                  /*fileDescriptorOffset=*/ assetFileDescriptor.getStartOffset(),
                  options);
            }
          },
          libName);
    }
  }

  /**
   * Initializes the JNI and returns C++ handle by first loading the C++ library and then invokes
   * {@link EmptyHandleProvider#createHandle()}.
   *
   * @param provider provider to get C++ handle, usually returned from native call
   * @return C++ handle as long
   */
  public static long createHandleFromLibrary(EmptyHandleProvider provider, String libName) {
    tryLoadLibrary(libName);
    try {
      return provider.createHandle();
    } catch (RuntimeException e) {
      String errorMessage = "Error getting native address of native library: " + libName;
      Log.e(TAG, errorMessage, e);
      throw new IllegalStateException(errorMessage, e);
    }
  }

  /**
   * Initializes the JNI and returns C++ handle by first loading the C++ library and then invokes
   * {@link MultipleBuffersHandleProvider#createHandle(ByteBuffer...)}.
   *
   * @param context app context
   * @param provider provider to get C++ pointer, usually returned from native call
   * @param libName name of C++ lib to load
   * @param filePaths file paths to load
   * @return C++ pointer as long
   * @throws IOException If model file fails to load.
   */
  public static long createHandleWithMultipleAssetFilesFromLibrary(
      Context context,
      final MultipleBuffersHandleProvider provider,
      String libName,
      String... filePaths)
      throws IOException {
    final MappedByteBuffer[] buffers = new MappedByteBuffer[filePaths.length];
    for (int i = 0; i < filePaths.length; i++) {
      buffers[i] = loadMappedFile(context, filePaths[i]);
    }
    return createHandleFromLibrary(
        new EmptyHandleProvider() {
          @Override
          public long createHandle() {
            return provider.createHandle(buffers);
          }
        },
        libName);
  }

  /**
   * Loads a file from the asset folder through memory mapping.
   *
   * @param context Application context to access assets.
   * @param filePath Asset path of the file.
   * @return the loaded memory mapped file.
   * @throws IOException If model file fails to load.
   */
  public static MappedByteBuffer loadMappedFile(Context context, String filePath)
      throws IOException {
    try (AssetFileDescriptor fileDescriptor = context.getAssets().openFd(filePath);
        FileInputStream inputStream = new FileInputStream(fileDescriptor.getFileDescriptor())) {
      FileChannel fileChannel = inputStream.getChannel();
      long startOffset = fileDescriptor.getStartOffset();
      long declaredLength = fileDescriptor.getDeclaredLength();
      return fileChannel.map(FileChannel.MapMode.READ_ONLY, startOffset, declaredLength);
    }
  }

  /**
   * Try loading a native library, if it's already loaded return directly.
   *
   * @param libName name of the lib
   */
  public static void tryLoadLibrary(String libName) {
    try {
      System.loadLibrary(libName);
    } catch (UnsatisfiedLinkError e) {
      String errorMessage = "Error loading native library: " + libName;
      Log.e(TAG, errorMessage, e);
      throw new UnsatisfiedLinkError(errorMessage);
    }
  }

  public static long createProtoBaseOptionsHandle(BaseOptions baseOptions) {
    return createProtoBaseOptionsHandleWithLegacyNumThreads(baseOptions, /*legacyNumThreads =*/ -1);
  }

  public static long createProtoBaseOptionsHandleWithLegacyNumThreads(
      BaseOptions baseOptions, int legacyNumThreads) {
    // NumThreads should be configured through BaseOptions. However, if NumThreads is configured
    // through the legacy API of the Task Java API (then it will not equal to -1, the default
    // value), use it to overide the one in baseOptions.
    return createProtoBaseOptions(
        baseOptions.getComputeSettings().getDelegate().getValue(),
        legacyNumThreads == -1 ? baseOptions.getNumThreads() : legacyNumThreads);
  }

  private TaskJniUtils() {}

  private static native long createProtoBaseOptions(int delegate, int numThreads);
}
