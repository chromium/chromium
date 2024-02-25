/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.support.tensorbuffer;

import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkArgument;
import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkNotNull;
import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkState;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import org.checkerframework.checker.nullness.qual.NonNull;
import org.tensorflow.lite.DataType;

/** Represents the data buffer for either a model's input or its output. */
public abstract class TensorBuffer {
  /** Where the data is stored. */
  protected ByteBuffer buffer;

  /** Shape of the tensor stored in this buffer. */
  protected int[] shape;

  /** Number of elements in the buffer. It will be changed to a proper value in the constructor. */
  protected int flatSize = -1;

  /**
   * Indicator of whether this buffer is dynamic or fixed-size. Fixed-size buffers will have
   * pre-allocated memory and fixed size. While the size of dynamic buffers can be changed.
   */
  protected final boolean isDynamic;

  /**
   * Creates a {@link TensorBuffer} with specified {@code shape} and {@link DataType}. Here are some
   * examples:
   *
   * <pre>
   * // Creating a float TensorBuffer with shape {2, 3}:
   * int[] shape = new int[] {2, 3};
   * TensorBuffer tensorBuffer = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
   * </pre>
   *
   * <pre>
   * // Creating an uint8 TensorBuffer of a scalar:
   * int[] shape = new int[] {};
   * TensorBuffer tensorBuffer = TensorBuffer.createFixedSize(shape, DataType.UINT8);
   * </pre>
   *
   * <pre>
   * // Creating an empty uint8 TensorBuffer:
   * int[] shape = new int[] {0};
   * TensorBuffer tensorBuffer = TensorBuffer.createFixedSize(shape, DataType.UINT8);
   * </pre>
   *
   * <p>The size of a fixed-size TensorBuffer cannot be changed once it is created.
   *
   * @param shape The shape of the {@link TensorBuffer} to be created.
   * @param dataType The dataType of the {@link TensorBuffer} to be created.
   * @throws NullPointerException if {@code shape} is null.
   * @throws IllegalArgumentException if {@code shape} has non-positive elements.
   */
  @NonNull
  public static TensorBuffer createFixedSize(@NonNull int[] shape, DataType dataType) {
    switch (dataType) {
      case FLOAT32:
        return new TensorBufferFloat(shape);
      case UINT8:
        return new TensorBufferUint8(shape);
      default:
        throw new AssertionError("TensorBuffer does not support data type: " + dataType);
    }
  }

  /**
   * Creates an empty dynamic {@link TensorBuffer} with specified {@link DataType}. The shape of the
   * created {@link TensorBuffer} is {0}.
   *
   * <p>Dynamic TensorBuffers will reallocate memory when loading arrays or data buffers of
   * different buffer sizes. Here are some examples:
   *
   * <pre>
   * // Creating a float dynamic TensorBuffer:
   * TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
   * // Loading a float array:
   * float[] arr1 = new float[] {1, 2, 3};
   * tensorBuffer.loadArray(arr, new int[] {arr1.length});
   * // loading another float array:
   * float[] arr2 = new float[] {1, 2, 3, 4, 5};
   * tensorBuffer.loadArray(arr, new int[] {arr2.length});
   * // loading a third float array with the same size as arr2, assuming shape doesn't change:
   * float[] arr3 = new float[] {5, 4, 3, 2, 1};
   * tensorBuffer.loadArray(arr);
   * // loading a forth float array with different size as arr3 and omitting the shape will result
   * // in error:
   * float[] arr4 = new float[] {3, 2, 1};
   * tensorBuffer.loadArray(arr); // Error: The size of byte buffer and the shape do not match.
   * </pre>
   *
   * @param dataType The dataType of the {@link TensorBuffer} to be created.
   */
  @NonNull
  public static TensorBuffer createDynamic(DataType dataType) {
    switch (dataType) {
      case FLOAT32:
        return new TensorBufferFloat();
      case UINT8:
        return new TensorBufferUint8();
      default:
        throw new AssertionError("TensorBuffer does not support data type: " + dataType);
    }
  }

  /**
   * Creates a {@link TensorBuffer} deep-copying data from another, with specified {@link DataType}.
   *
   * @param buffer the source {@link TensorBuffer} to copy from.
   * @param dataType the expected {@link DataType} of newly created {@link TensorBuffer}.
   * @throws NullPointerException if {@code buffer} is null.
   */
  @NonNull
  public static TensorBuffer createFrom(@NonNull TensorBuffer buffer, DataType dataType) {
    checkNotNull(buffer, "Cannot create a buffer from null");
    TensorBuffer result;
    if (buffer.isDynamic()) {
      result = createDynamic(dataType);
    } else {
      result = createFixedSize(buffer.shape, dataType);
    }
    // The only scenario we need float array is FLOAT32->FLOAT32, or we can always use INT as
    // intermediate container.
    // The assumption is not true when we support other data types.
    if (buffer.getDataType() == DataType.FLOAT32 && dataType == DataType.FLOAT32) {
      float[] data = buffer.getFloatArray();
      result.loadArray(data, buffer.shape);
    } else {
      int[] data = buffer.getIntArray();
      result.loadArray(data, buffer.shape);
    }
    return result;
  }

  /** Returns the data buffer. */
  @NonNull
  public ByteBuffer getBuffer() {
    return buffer;
  }

  /**
   * Gets the flatSize of the buffer.
   *
   * @throws IllegalStateException if the underlying data is corrupted
   */
  public int getFlatSize() {
    assertShapeIsCorrect();
    return flatSize;
  }

  /**
   * Gets the current shape. (returning a copy here to avoid unexpected modification.)
   *
   * @throws IllegalStateException if the underlying data is corrupted
   */
  @NonNull
  public int[] getShape() {
    assertShapeIsCorrect();
    return Arrays.copyOf(shape, shape.length);
  }

  /** Returns the data type of this buffer. */
  public abstract DataType getDataType();

  /**
   * Returns a float array of the values stored in this buffer. If the buffer is of different types
   * than float, the values will be converted into float. For example, values in {@link
   * TensorBufferUint8} will be converted from uint8 to float.
   */
  @NonNull
  public abstract float[] getFloatArray();

  /**
   * Returns a float value at a given index. If the buffer is of different types than float, the
   * value will be converted into float. For example, when reading a value from {@link
   * TensorBufferUint8}, the value will be first read out as uint8, and then will be converted from
   * uint8 to float.
   *
   * <pre>
   * For example, a TensorBuffer with shape {2, 3} that represents the following array,
   * [[0.0f, 1.0f, 2.0f], [3.0f, 4.0f, 5.0f]].
   *
   * The fourth element (whose value is 3.0f) in the TensorBuffer can be retrieved by:
   * float v = tensorBuffer.getFloatValue(3);
   * </pre>
   *
   * @param absIndex The absolute index of the value to be read.
   */
  public abstract float getFloatValue(int absIndex);

  /**
   * Returns an int array of the values stored in this buffer. If the buffer is of different type
   * than int, the values will be converted into int, and loss of precision may apply. For example,
   * getting an int array from a {@link TensorBufferFloat} with values {400.32f, 23.04f}, the output
   * is {400, 23}.
   */
  @NonNull
  public abstract int[] getIntArray();

  /**
   * Returns an int value at a given index. If the buffer is of different types than int, the value
   * will be converted into int. For example, when reading a value from {@link TensorBufferFloat},
   * the value will be first read out as float, and then will be converted from float to int. Loss
   * of precision may apply.
   *
   * <pre>
   * For example, a TensorBuffer with shape {2, 3} that represents the following array,
   * [[0.0f, 1.0f, 2.0f], [3.0f, 4.0f, 5.0f]].
   *
   * The fourth element (whose value is 3.0f) in the TensorBuffer can be retrieved by:
   * int v = tensorBuffer.getIntValue(3);
   * Note that v is converted from 3.0f to 3 as a result of type conversion.
   * </pre>
   *
   * @param absIndex The absolute index of the value to be read.
   */
  public abstract int getIntValue(int absIndex);

  /**
   * Returns the number of bytes of a single element in the array. For example, a float buffer will
   * return 4, and a byte buffer will return 1.
   */
  public abstract int getTypeSize();

  /** Returns if the {@link TensorBuffer} is dynamic sized (could resize arbitrarily). */
  public boolean isDynamic() {
    return isDynamic;
  }

  /**
   * Loads an int array into this buffer with specific shape. If the buffer is of different types
   * than int, the values will be converted into the buffer's type before being loaded into the
   * buffer, and loss of precision may apply. For example, loading an int array with values {400,
   * -23} into a {@link TensorBufferUint8} , the values will be clamped to [0, 255] and then be
   * casted to uint8 by {255, 0}.
   *
   * @param src The source array to be loaded.
   * @param shape Shape of the tensor that {@code src} represents.
   * @throws NullPointerException if {@code src} is null.
   * @throws NullPointerException if {@code shape} is null.
   * @throws IllegalArgumentException if the size of the array to be loaded does not match the
   *     specified shape.
   */
  public abstract void loadArray(@NonNull int[] src, @NonNull int[] shape);

  /**
   * Loads an int array into this buffer. If the buffer is of different types than int, the values
   * will be converted into the buffer's type before being loaded into the buffer, and loss of
   * precision may apply. For example, loading an int array with values {400, -23} into a {@link
   * TensorBufferUint8} , the values will be clamped to [0, 255] and then be casted to uint8 by
   * {255, 0}.
   *
   * <p>Using this method assumes that the shape of {@code src} is the same as the shape of this
   * {@link TensorBuffer}. Thus the size of {@code buffer} ({@code src.length}) should always match
   * the flat size of this {@link TensorBuffer}, for both fixed-size and dynamic {@link
   * TensorBuffer}. Use {@link #loadArray(int[], int[])} if {@code src} has a different shape.
   *
   * @param src The source array to be loaded.
   */
  public void loadArray(@NonNull int[] src) {
    loadArray(src, shape);
  }

  /**
   * Loads a float array into this buffer with specific shape. If the buffer is of different types
   * than float, the values will be converted into the buffer's type before being loaded into the
   * buffer, and loss of precision may apply. For example, loading a float array into a {@link
   * TensorBufferUint8} with values {400.32f, -23.04f}, the values will be clamped to [0, 255] and
   * then be casted to uint8 by {255, 0}.
   *
   * @param src The source array to be loaded.
   * @param shape Shape of the tensor that {@code src} represents.
   * @throws NullPointerException if {@code src} is null.
   * @throws NullPointerException if {@code shape} is null.
   * @throws IllegalArgumentException if the size of the array to be loaded does not match the
   *     specified shape.
   */
  public abstract void loadArray(@NonNull float[] src, @NonNull int[] shape);

  /**
   * Loads a float array into this buffer. If the buffer is of different types than float, the
   * values will be converted into the buffer's type before being loaded into the buffer, and loss
   * of precision may apply. For example, loading a float array into a {@link TensorBufferUint8}
   * with values {400.32f, -23.04f}, the values will be clamped to [0, 255] and then be casted to
   * uint8 by {255, 0}.
   *
   * <p>Using this method assumes that the shape of {@code src} is the same as the shape of this
   * {@link TensorBuffer}. Thus the size of {@code buffer} ({@code src.length}) should always match
   * the flat size of this {@link TensorBuffer}, for both fixed-size and dynamic {@link
   * TensorBuffer}. Use {@link #loadArray(float[], int[])} if {@code src} has a different shape.
   *
   * @param src The source array to be loaded.
   */
  public void loadArray(@NonNull float[] src) {
    loadArray(src, shape);
  }

  /**
   * Loads a byte buffer into this {@link TensorBuffer} with specific shape.
   *
   * <p>Important: The loaded buffer is a reference. DO NOT MODIFY. We don't create a copy here for
   * performance concern, but if modification is necessary, please make a copy.
   *
   * <p>For the best performance, always load a direct {@link ByteBuffer} or a {@link ByteBuffer}
   * backed by an array.
   *
   * @param buffer The byte buffer to load.
   * @throws NullPointerException if {@code buffer} is null.
   * @throws IllegalArgumentException if the size of {@code buffer} and {@code typeSize} do not
   *     match or the size of {@code buffer} and {@code flatSize} do not match.
   */
  public void loadBuffer(@NonNull ByteBuffer buffer, @NonNull int[] shape) {
    checkNotNull(buffer, "Byte buffer cannot be null.");
    checkArgument(isShapeValid(shape), "Values in TensorBuffer shape should be non-negative.");

    int flatSize = computeFlatSize(shape);
    checkArgument(
        (buffer.limit() == getTypeSize() * flatSize),
        "The size of byte buffer and the shape do not match. Expected: "
            + getTypeSize() * flatSize
            + " Actual: "
            + buffer.limit());

    if (!isDynamic) {
      // Make sure the new shape fits the buffer size when TensorBuffer has fixed size.
      checkArgument(Arrays.equals(shape, this.shape));
    }

    // Update to the new shape, since shape dim values might change.
    this.shape = shape.clone();
    this.flatSize = flatSize;

    buffer.rewind();
    this.buffer = buffer;
  }

  /**
   * Loads a byte buffer into this {@link TensorBuffer}. Buffer size must match the flat size of
   * this {@link TensorBuffer}.
   *
   * <p>Using this method assumes that the shape of {@code buffer} is the same as the shape of this
   * {@link TensorBuffer}. Thus the size of {@code buffer} ({@code buffer.limit()}) should always
   * match the flat size of this {@link TensorBuffer}, for both fixed-size and dynamic {@link
   * TensorBuffer}. Use {@link #loadBuffer(ByteBuffer, int[])} if {@code buffer} has a different
   * shape.
   *
   * <p>Important: The loaded buffer is a reference. DO NOT MODIFY. We don't create a copy here for
   * performance concern, but if modification is necessary, please make a copy.
   *
   * <p>For the best performance, always load a direct {@link ByteBuffer} or a {@link ByteBuffer}
   * backed by an array.
   *
   * <p>If the {@code buffer} is read-only, we adopt a copy-on-write strategy for performance.
   *
   * @param buffer The byte buffer to load.
   */
  public void loadBuffer(@NonNull ByteBuffer buffer) {
    loadBuffer(buffer, shape);
  }

  /**
   * Constructs a fixed size {@link TensorBuffer} with specified {@code shape}.
   *
   * @throws NullPointerException if {@code shape} is null.
   * @throws IllegalArgumentException if {@code shape} has non-positive elements.
   */
  protected TensorBuffer(@NonNull int[] shape) {
    isDynamic = false;
    allocateMemory(shape);
  }

  /** Constructs a dynamic {@link TensorBuffer} which can be resized. */
  protected TensorBuffer() {
    isDynamic = true;
    // Initialize the dynamic TensorBuffer with an empty ByteBuffer.
    allocateMemory(new int[] {0});
  }

  /** Calculates number of elements in the buffer. */
  protected static int computeFlatSize(@NonNull int[] shape) {
    checkNotNull(shape, "Shape cannot be null.");
    int prod = 1;
    for (int s : shape) {
      prod = prod * s;
    }
    return prod;
  }

  /**
   * For dynamic buffer, resize the memory if needed. For fixed-size buffer, check if the {@code
   * shape} of src fits the buffer size.
   */
  protected void resize(@NonNull int[] shape) {
    if (isDynamic) {
      allocateMemory(shape);
    } else {
      // Make sure the new shape fits the buffer size when TensorBuffer has fixed size.
      checkArgument(Arrays.equals(shape, this.shape));
      this.shape = shape.clone();
    }
  }

  /** Copies the underlying {@link ByteBuffer} if it's readonly. */
  protected synchronized void copyByteBufferIfReadOnly() {
    if (!buffer.isReadOnly()) {
      return;
    }
    ByteBuffer newByteBuffer = ByteBuffer.allocateDirect(buffer.capacity());
    newByteBuffer.order(buffer.order());
    newByteBuffer.put(buffer);
    newByteBuffer.rewind();
    buffer = newByteBuffer;
  }

  /**
   * Allocates buffer with corresponding size of the {@code shape}. If shape is an empty array, this
   * {@link TensorBuffer} will be created as a scalar and its flatSize will be 1.
   *
   * @throws NullPointerException if {@code shape} is null.
   * @throws IllegalArgumentException if {@code shape} has negative elements.
   */
  private void allocateMemory(@NonNull int[] shape) {
    checkNotNull(shape, "TensorBuffer shape cannot be null.");
    checkArgument(isShapeValid(shape), "Values in TensorBuffer shape should be non-negative.");

    // Check if the new shape is the same as current shape.
    int newFlatSize = computeFlatSize(shape);
    this.shape = shape.clone();
    if (flatSize == newFlatSize) {
      return;
    }

    // Update to the new shape.
    flatSize = newFlatSize;
    buffer = ByteBuffer.allocateDirect(flatSize * getTypeSize());
    buffer.order(ByteOrder.nativeOrder());
  }

  /**
   * Verifies if the shape of the {@link TensorBuffer} matched the size of the underlying {@link
   * ByteBuffer}.
   */
  private void assertShapeIsCorrect() {
    int flatSize = computeFlatSize(shape);
    checkState(
        (buffer.limit() == getTypeSize() * flatSize),
        String.format(
            "The size of underlying ByteBuffer (%d) and the shape (%s) do not match. The"
                + " ByteBuffer may have been changed.",
            buffer.limit(), Arrays.toString(shape)));
  }

  /**
   * Checks if {@code shape} meets one of following two requirements: 1. Elements in {@code shape}
   * are all non-negative numbers. 2. {@code shape} is an empty array, which corresponds to scalar.
   */
  private static boolean isShapeValid(@NonNull int[] shape) {
    if (shape.length == 0) {
      // This shape refers to a scalar.
      return true;
    }

    // This shape refers to a multidimensional array.
    for (int s : shape) {
      // All elements in shape should be non-negative.
      if (s < 0) {
        return false;
      }
    }
    return true;
  }
}
