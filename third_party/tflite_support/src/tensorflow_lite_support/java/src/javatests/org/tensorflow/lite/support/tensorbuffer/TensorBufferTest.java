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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;

/** Test helper class for inserting and retrieving arrays. */
class ArrayTestRunner {
  // List of TensorBuffer types to be tested.
  private static final DataType[] BUFFER_TYPE_LIST = {DataType.FLOAT32, DataType.UINT8};
  // List of source arrays to be loaded into TensorBuffer during the tests.
  private final ArrayList<Object> srcArrays;
  // List of array data type with respect to srcArrays.
  private final ArrayList<DataType> arrDataTypes;
  // List of array shape with respect to srcArrays.
  private final ArrayList<int[]> arrShapes;
  private final int[] tensorBufferShape;
  private final ExpectedResults expectedResForFloatBuf;
  private final ExpectedResults expectedResForByteBuf;

  public ArrayTestRunner(Builder builder) {
    if (builder.srcArrays.size() != builder.arrDataTypes.size()) {
      throw new IllegalArgumentException(
          "Number of source arrays and number of data types do not match.");
    }

    this.srcArrays = builder.srcArrays;
    this.arrDataTypes = builder.arrDataTypes;
    this.arrShapes = builder.arrShapes;
    this.tensorBufferShape = builder.tensorBufferShape;
    this.expectedResForFloatBuf = builder.expectedResForFloatBuf;
    this.expectedResForByteBuf = builder.expectedResForByteBuf;
  }

  static class ExpectedResults {
    public float[] floatArr;
    public int[] intArr;
    public int[] shape;
  }

  public static class Builder {
    private final ArrayList<Object> srcArrays = new ArrayList<>();
    private final ArrayList<DataType> arrDataTypes = new ArrayList<>();
    private final ArrayList<int[]> arrShapes = new ArrayList<>();
    private int[] tensorBufferShape;
    private final ExpectedResults expectedResForFloatBuf = new ExpectedResults();
    private final ExpectedResults expectedResForByteBuf = new ExpectedResults();

    public static Builder newInstance() {
      return new Builder();
    }

    private Builder() {}

    /** Loads a test array into the test runner. */
    public Builder addSrcArray(Object src, int[] shape) {
      // src should be a primitive 1D array.
      DataType dataType = dataTypeOfArray(src);
      switch (dataType) {
        case INT32:
        case FLOAT32:
          srcArrays.add(src);
          arrDataTypes.add(dataType);
          arrShapes.add(shape);
          return this;
        default:
          throw new AssertionError("Cannot resolve srouce arrays in the DataType of " + dataType);
      }
    }

    public Builder setTensorBufferShape(int[] tensorBufferShape) {
      this.tensorBufferShape = tensorBufferShape;
      return this;
    }

    public Builder setExpectedResults(
        DataType bufferType, float[] expectedFloatArr, int[] expectedIntArr) {
      ExpectedResults er;
      switch (bufferType) {
        case UINT8:
          er = expectedResForByteBuf;
          break;
        case FLOAT32:
          er = expectedResForFloatBuf;
          break;
        default:
          throw new AssertionError("Cannot test TensorBuffer in the DataType of " + bufferType);
      }

      er.floatArr = expectedFloatArr;
      er.intArr = expectedIntArr;
      return this;
    }

    public ArrayTestRunner build() {
      int[] expectedShape;
      if (arrShapes.isEmpty()) {
        // If no array will be loaded, the array is an empty array.
        expectedShape = new int[] {0};
      } else {
        expectedShape = arrShapes.get(arrShapes.size() - 1);
      }
      expectedResForByteBuf.shape = expectedShape;
      expectedResForFloatBuf.shape = expectedShape;
      return new ArrayTestRunner(this);
    }
  }

  public static DataType[] getBufferTypeList() {
    return BUFFER_TYPE_LIST;
  }

  /**
   * Runs tests in the following steps: 1. Create a TensorBuffer. If tensorBufferShape is null,
   * create a dynamic buffer. Otherwise, create a fixed-size buffer accordingly. 2. Load arrays in
   * srcArrays one by one into the TensotBuffer. 3. Get arrays for each supported primitive types in
   * TensorBuffer, such as int array and float array for now. Check if the results are correct. 4.
   * Repeat Step 1 to 3 for all buffer types in BUFFER_TYPE_LIST.
   */
  public void run() {
    for (DataType bufferDataType : BUFFER_TYPE_LIST) {
      TensorBuffer tensorBuffer;
      if (tensorBufferShape == null) {
        tensorBuffer = TensorBuffer.createDynamic(bufferDataType);
      } else {
        tensorBuffer = TensorBuffer.createFixedSize(tensorBufferShape, bufferDataType);
      }
      for (int i = 0; i < srcArrays.size(); i++) {
        switch (arrDataTypes.get(i)) {
          case INT32:
            int[] arrInt = (int[]) srcArrays.get(i);
            tensorBuffer.loadArray(arrInt, arrShapes.get(i));
            break;
          case FLOAT32:
            float[] arrFloat = (float[]) srcArrays.get(i);
            tensorBuffer.loadArray(arrFloat, arrShapes.get(i));
            break;
          default:
            break;
        }
      }
      checkResults(tensorBuffer);
    }
  }

  private void checkResults(TensorBuffer tensorBuffer) {
    ExpectedResults er;
    switch (tensorBuffer.getDataType()) {
      case UINT8:
        er = expectedResForByteBuf;
        break;
      case FLOAT32:
        er = expectedResForFloatBuf;
        break;
      default:
        throw new AssertionError(
            "Cannot test TensorBuffer in the DataType of " + tensorBuffer.getDataType());
    }

    // Checks getIntArray() and getFloatArray().
    int[] resIntArr = tensorBuffer.getIntArray();
    assertThat(resIntArr).isEqualTo(er.intArr);
    float[] resFloatArr = tensorBuffer.getFloatArray();
    assertThat(resFloatArr).isEqualTo(er.floatArr);
    assertThat(tensorBuffer.getShape()).isEqualTo(er.shape);

    // Checks getIntValue(int index) and getFloatValue(int index).
    int flatSize = tensorBuffer.getFlatSize();
    float[] resFloatValues = new float[flatSize];
    int[] resIntValues = new int[flatSize];
    for (int i = 0; i < flatSize; i++) {
      resFloatValues[i] = tensorBuffer.getFloatValue(i);
      resIntValues[i] = tensorBuffer.getIntValue(i);
    }
    assertThat(resFloatValues).isEqualTo(er.floatArr);
    assertThat(resIntValues).isEqualTo(er.intArr);
  }

  /** Gets the data type of an 1D array. */
  private static DataType dataTypeOfArray(Object arr) {
    if (arr != null) {
      Class<?> c = arr.getClass();
      if (c.isArray()) {
        c = c.getComponentType();
        if (float.class.equals(c)) {
          return DataType.FLOAT32;
        } else if (int.class.equals(c)) {
          return DataType.INT32;
        } else if (byte.class.equals(c)) {
          return DataType.UINT8;
        } else if (long.class.equals(c)) {
          return DataType.INT64;
        } else if (String.class.equals(c)) {
          return DataType.STRING;
        }
      }
    }
    throw new IllegalArgumentException(
        "Requires a 1D array. Cannot resolve data type of " + arr.getClass().getName());
  }
}

/** Tests of {@link org.tensorflow.lite.support.tensorbuffer.TensorBuffer}. */
@RunWith(RobolectricTestRunner.class)
public final class TensorBufferTest {
  // FLOAT_ARRAY1 and INT_ARRAY1 correspond to each other.
  private static final int[] ARRAY1_SHAPE = new int[] {2, 3};
  private static final float[] FLOAT_ARRAY1 = new float[] {500.1f, 4.2f, 3.3f, 2.4f, 1.5f, 6.1f};
  private static final float[] FLOAT_ARRAY1_ROUNDED =
      new float[] {500.0f, 4.0f, 3.0f, 2.0f, 1.0f, 6.0f};
  // FLOAT_ARRAY1_CAPPED and INT_ARRAY1_CAPPED correspond to the expected values when converted into
  // uint8.
  private static final float[] FLOAT_ARRAY1_CAPPED =
      new float[] {255.0f, 4.0f, 3.0f, 2.0f, 1.0f, 6.0f};
  private static final int[] INT_ARRAY1 = new int[] {500, 4, 3, 2, 1, 6};
  private static final int[] INT_ARRAY1_CAPPED = new int[] {255, 4, 3, 2, 1, 6};
  // FLOAT_ARRAY2 and INT_ARRAY2 correspond to each other.
  private static final int[] ARRAY2_SHAPE = new int[] {2, 1};
  private static final float[] FLOAT_ARRAY2 = new float[] {6.7f, 7.6f};
  private static final float[] FLOAT_ARRAY2_ROUNDED = new float[] {6.0f, 7.0f};
  private static final int[] INT_ARRAY2 = new int[] {6, 7};
  // FLOAT_ARRAY2 and FLOAT_ARRAY3 have the same size.
  private static final int[] ARRAY3_SHAPE = new int[] {2, 1};
  private static final float[] FLOAT_ARRAY3 = new float[] {8.2f, 9.9f};
  private static final float[] FLOAT_ARRAY3_ROUNDED = new float[] {8.0f, 9.0f};
  // INT_ARRAY2 and INT_ARRAY3 have the same size.
  private static final int[] INT_ARRAY3 = new int[] {8, 9};
  private static final int[] EMPTY_ARRAY_SHAPE = new int[] {0};
  private static final int[] EMPTY_INT_ARRAY = new int[0];
  private static final float[] EMPTY_FLOAT_ARRAY = new float[0];
  // Single element array which represents a scalar.
  private static final int[] SCALAR_ARRAY_SHAPE = new int[] {};
  private static final float[] FLOAT_SCALAR_ARRAY = new float[] {800.2f};
  private static final float[] FLOAT_SCALAR_ARRAY_ROUNDED = new float[] {800.0f};
  private static final float[] FLOAT_SCALAR_ARRAY_CAPPED = new float[] {255.0f};
  private static final int[] INT_SCALAR_ARRAY = new int[] {800};
  private static final int[] INT_SCALAR_ARRAY_CAPPED = new int[] {255};
  // Several different ByteBuffer.
  private static final ByteBuffer EMPTY_BYTE_BUFFER = ByteBuffer.allocateDirect(0);
  private static final ByteBuffer FLOAT_BYTE_BUFFER1 = ByteBuffer.allocateDirect(24);

  static {
    FLOAT_BYTE_BUFFER1.rewind();

    FloatBuffer floatBuffer = FLOAT_BYTE_BUFFER1.asFloatBuffer();
    floatBuffer.put(FLOAT_ARRAY1);
  }

  private static final ByteBuffer INT_BYTE_BUFFER2 = ByteBuffer.allocateDirect(2);

  static {
    INT_BYTE_BUFFER2.rewind();

    for (int a : INT_ARRAY2) {
      INT_BYTE_BUFFER2.put((byte) a);
    }
  }

  @Test
  public void testCreateFixedSizeTensorBufferFloat() {
    int[] shape = new int[] {1, 2, 3};
    TensorBuffer tensorBufferFloat = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    assertThat(tensorBufferFloat).isNotNull();
    assertThat(tensorBufferFloat.getFlatSize()).isEqualTo(6);
  }

  @Test
  public void testCreateFixedSizeTensorBufferUint8() {
    int[] shape = new int[] {1, 2, 3};
    TensorBuffer tensorBufferUint8 = TensorBuffer.createFixedSize(shape, DataType.UINT8);
    assertThat(tensorBufferUint8).isNotNull();
    assertThat(tensorBufferUint8.getFlatSize()).isEqualTo(6);
  }

  @Test
  public void testCreateDynamicTensorBufferFloat() {
    TensorBuffer tensorBufferFloat = TensorBuffer.createDynamic(DataType.FLOAT32);
    assertThat(tensorBufferFloat).isNotNull();
  }

  @Test
  public void testCreateDynamicTensorBufferUint8() {
    TensorBuffer tensorBufferUint8 = TensorBuffer.createDynamic(DataType.UINT8);
    assertThat(tensorBufferUint8).isNotNull();
  }

  @Test
  public void testCreateTensorBufferFromFixedSize() {
    int[] shape = new int[] {1, 2, 3};
    TensorBuffer src = TensorBuffer.createFixedSize(shape, DataType.UINT8);
    TensorBuffer dst = TensorBuffer.createFrom(src, DataType.FLOAT32);
    assertThat(dst.getShape()).isEqualTo(new int[] {1, 2, 3});
  }

  @Test
  public void testCreateTensorBufferFromDynamicSize() {
    int[] shape = new int[] {1, 2, 3};
    TensorBuffer src = TensorBuffer.createDynamic(DataType.UINT8);
    src.resize(shape);
    TensorBuffer dst = TensorBuffer.createFrom(src, DataType.FLOAT32);
    assertThat(dst.getShape()).isEqualTo(new int[] {1, 2, 3});
  }

  @Test
  public void testCreateTensorBufferUInt8FromUInt8() {
    int[] shape = new int[] {INT_ARRAY1.length};
    TensorBuffer src = TensorBuffer.createFixedSize(shape, DataType.UINT8);
    src.loadArray(INT_ARRAY1);
    TensorBuffer dst = TensorBuffer.createFrom(src, DataType.UINT8);
    int[] data = dst.getIntArray();
    assertThat(data).isEqualTo(INT_ARRAY1_CAPPED);
  }

  @Test
  public void testCreateTensorBufferUInt8FromFloat32() {
    TensorBuffer src = TensorBuffer.createDynamic(DataType.FLOAT32);
    src.loadArray(FLOAT_ARRAY1, ARRAY1_SHAPE);
    TensorBuffer dst = TensorBuffer.createFrom(src, DataType.UINT8);
    int[] data = dst.getIntArray();
    assertThat(data).isEqualTo(INT_ARRAY1_CAPPED);
  }

  @Test
  public void testCreateTensorBufferFloat32FromUInt8() {
    TensorBuffer src = TensorBuffer.createDynamic(DataType.UINT8);
    src.loadArray(INT_ARRAY1, ARRAY1_SHAPE);
    TensorBuffer dst = TensorBuffer.createFrom(src, DataType.FLOAT32);
    float[] data = dst.getFloatArray();
    assertThat(data).isEqualTo(FLOAT_ARRAY1_CAPPED);
  }

  @Test
  public void testCreateTensorBufferFloat32FromFloat32() {
    int[] shape = new int[] {FLOAT_ARRAY1.length};
    TensorBuffer src = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    src.loadArray(FLOAT_ARRAY1);
    TensorBuffer dst = TensorBuffer.createFrom(src, DataType.FLOAT32);
    float[] data = dst.getFloatArray();
    assertThat(data).isEqualTo(FLOAT_ARRAY1);
  }

  @Test
  public void testGetBuffer() throws IOException {
    int[] shape = new int[] {1, 2, 3};
    TensorBuffer tensorBufferUint8 = TensorBuffer.createFixedSize(shape, DataType.UINT8);
    assertThat(tensorBufferUint8.getBuffer()).isNotNull();
  }

  @Test
  public void testLoadAndGetIntArrayWithFixedSizeForScalarArray() throws IOException {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(INT_SCALAR_ARRAY, SCALAR_ARRAY_SHAPE)
        .setTensorBufferShape(SCALAR_ARRAY_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_SCALAR_ARRAY_ROUNDED,
            /*expectedIntArr=*/ INT_SCALAR_ARRAY)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_SCALAR_ARRAY_CAPPED,
            /*expectedIntArr=*/ INT_SCALAR_ARRAY_CAPPED)
        .build()
        .run();
  }

  @Test
  public void testLoadAndGetFloatArrayWithFixedSizeForScalarArray() throws IOException {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(FLOAT_SCALAR_ARRAY, SCALAR_ARRAY_SHAPE)
        .setTensorBufferShape(SCALAR_ARRAY_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_SCALAR_ARRAY,
            /*expectedIntArr=*/ INT_SCALAR_ARRAY)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_SCALAR_ARRAY_CAPPED,
            /*expectedIntArr=*/ INT_SCALAR_ARRAY_CAPPED)
        .build()
        .run();
  }

  @Test
  public void testLoadAndGetIntArrayWithFixedSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(INT_ARRAY1, ARRAY1_SHAPE)
        .setTensorBufferShape(ARRAY1_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY1_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY1)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY1_CAPPED,
            /*expectedIntArr=*/ INT_ARRAY1_CAPPED)
        .build()
        .run();
  }

  @Test
  public void testLoadAndGetFloatArrayWithFixedSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(FLOAT_ARRAY1, ARRAY1_SHAPE)
        .setTensorBufferShape(ARRAY1_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY1,
            /*expectedIntArr=*/ INT_ARRAY1)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY1_CAPPED,
            /*expectedIntArr=*/ INT_ARRAY1_CAPPED)
        .build()
        .run();
  }

  @Test
  public void testRepeatedLoadAndGetIntArrayWithSameFixedSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(INT_ARRAY2, ARRAY2_SHAPE)
        .addSrcArray(INT_ARRAY3, ARRAY3_SHAPE)
        .setTensorBufferShape(ARRAY2_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY3_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY3)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY3_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY3)
        .build()
        .run();
  }

  @Test
  public void testRepeatedLoadAndGetFloatArrayWithSameFixedSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(FLOAT_ARRAY2, ARRAY2_SHAPE)
        .addSrcArray(FLOAT_ARRAY3, ARRAY3_SHAPE)
        .setTensorBufferShape(ARRAY2_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY3,
            /*expectedIntArr=*/ INT_ARRAY3)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY3_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY3)
        .build()
        .run();
  }

  @Test
  public void testRepeatedLoadIntArrayWithDifferentFixedSize() {
    int[] srcArr1 = INT_ARRAY1;
    int[] srcArr2 = INT_ARRAY2;
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer =
          TensorBuffer.createFixedSize(new int[] {srcArr1.length}, dataType);
      tensorBuffer.loadArray(srcArr1, new int[] {srcArr1.length});
      // Load srcArr2 which had different size as srcArr1.
      Assert.assertThrows(
          IllegalArgumentException.class,
          () -> tensorBuffer.loadArray(srcArr2, new int[] {srcArr2.length}));
    }
  }

  @Test
  public void testRepeatedLoadFloatArrayWithDifferentFixedSize() {
    float[] srcArr1 = FLOAT_ARRAY1;
    float[] srcArr2 = FLOAT_ARRAY2;
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer =
          TensorBuffer.createFixedSize(new int[] {srcArr1.length}, dataType);
      tensorBuffer.loadArray(srcArr1, new int[] {srcArr1.length});
      // Load srcArr2 which had different size as srcArr1.
      Assert.assertThrows(
          IllegalArgumentException.class,
          () -> tensorBuffer.loadArray(srcArr2, new int[] {srcArr2.length}));
    }
  }

  @Test
  public void testLoadAndGetIntArrayWithDynamicSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(INT_ARRAY1, ARRAY1_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY1_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY1)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY1_CAPPED,
            /*expectedIntArr=*/ INT_ARRAY1_CAPPED)
        .build()
        .run();
  }

  @Test
  public void testLoadAndGetFloatArrayWithDynamicSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(FLOAT_ARRAY1, ARRAY1_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY1,
            /*expectedIntArr=*/ INT_ARRAY1)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY1_CAPPED,
            /*expectedIntArr=*/ INT_ARRAY1_CAPPED)
        .build()
        .run();
  }

  @Test
  public void testRepeatedLoadAndGetIntArrayWithDifferentDynamicSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(INT_ARRAY1, ARRAY1_SHAPE)
        .addSrcArray(INT_ARRAY2, ARRAY2_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY2_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY2)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY2_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY2)
        .build()
        .run();
  }

  @Test
  public void testRepeatedLoadAndGetFloatArrayWithDifferentDynamicSize() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(FLOAT_ARRAY1, ARRAY1_SHAPE)
        .addSrcArray(FLOAT_ARRAY2, ARRAY2_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ FLOAT_ARRAY2,
            /*expectedIntArr=*/ INT_ARRAY2)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ FLOAT_ARRAY2_ROUNDED,
            /*expectedIntArr=*/ INT_ARRAY2)
        .build()
        .run();
  }

  @Test
  public void testGetForEmptyArrayWithFixedSizeBuffer() {
    ArrayTestRunner.Builder.newInstance()
        .setTensorBufferShape(EMPTY_ARRAY_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ EMPTY_FLOAT_ARRAY,
            /*expectedIntArr=*/ EMPTY_INT_ARRAY)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ EMPTY_FLOAT_ARRAY,
            /*expectedIntArr=*/ EMPTY_INT_ARRAY)
        .build()
        .run();
  }

  @Test
  public void testGetForEmptyArrayWithDynamicBuffer() {
    ArrayTestRunner.Builder.newInstance()
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ EMPTY_FLOAT_ARRAY,
            /*expectedIntArr=*/ EMPTY_INT_ARRAY)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ EMPTY_FLOAT_ARRAY,
            /*expectedIntArr=*/ EMPTY_INT_ARRAY)
        .build()
        .run();
  }

  @Test
  public void testRepeatedLoadAndGetForEmptyArray() {
    ArrayTestRunner.Builder.newInstance()
        .addSrcArray(EMPTY_INT_ARRAY, EMPTY_ARRAY_SHAPE)
        .addSrcArray(FLOAT_ARRAY2, ARRAY2_SHAPE)
        .addSrcArray(EMPTY_FLOAT_ARRAY, EMPTY_ARRAY_SHAPE)
        .setExpectedResults(
            /*bufferType = */ DataType.FLOAT32,
            /*expectedFloatArr=*/ EMPTY_FLOAT_ARRAY,
            /*expectedIntArr=*/ EMPTY_INT_ARRAY)
        .setExpectedResults(
            /*bufferType = */ DataType.UINT8,
            /*expectedFloatArr=*/ EMPTY_FLOAT_ARRAY,
            /*expectedIntArr=*/ EMPTY_INT_ARRAY)
        .build()
        .run();
  }

  @Test
  public void testLoadNullIntArrays() {
    int[] nullArray = null;
    int[] shape = new int[] {};
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      Assert.assertThrows(
          NullPointerException.class, () -> tensorBuffer.loadArray(nullArray, shape));
    }
  }

  @Test
  public void testLoadNullFloatArrays() {
    float[] nullArray = null;
    int[] shape = new int[] {};
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      Assert.assertThrows(
          NullPointerException.class, () -> tensorBuffer.loadArray(nullArray, shape));
    }
  }

  @Test
  public void testLoadFloatArraysWithNullShape() {
    float[] arr = new float[] {1.0f};
    int[] nullShape = null;
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      Assert.assertThrows(NullPointerException.class, () -> tensorBuffer.loadArray(arr, nullShape));
    }
  }

  @Test
  public void testLoadIntArraysWithNullShape() {
    int[] arr = new int[] {1};
    int[] nullShape = null;
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      Assert.assertThrows(NullPointerException.class, () -> tensorBuffer.loadArray(arr, nullShape));
    }
  }

  @Test
  public void testLoadIntArraysWithoutShapeAndArrayDoesNotMatchShape() {
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer fixedTensorBuffer = TensorBuffer.createFixedSize(ARRAY1_SHAPE, dataType);
      Assert.assertThrows(
          IllegalArgumentException.class, () -> fixedTensorBuffer.loadArray(INT_ARRAY2));
    }
  }

  @Test
  public void testLoadFloatArraysWithoutShapeAndArrayDoesNotMatchShape() {
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer fixedTensorBuffer = TensorBuffer.createFixedSize(ARRAY1_SHAPE, dataType);
      Assert.assertThrows(
          IllegalArgumentException.class, () -> fixedTensorBuffer.loadArray(FLOAT_ARRAY2));
    }
  }

  @Test
  public void testLoadByteBufferForNullBuffer() {
    ByteBuffer byteBuffer = null;
    int[] shape = new int[] {};
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      Assert.assertThrows(
          NullPointerException.class, () -> tensorBuffer.loadBuffer(byteBuffer, shape));
    }
  }

  @Test
  public void testLoadByteBufferForEmptyBuffer() {
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      tensorBuffer.loadBuffer(EMPTY_BYTE_BUFFER, EMPTY_ARRAY_SHAPE);
      assertThat(tensorBuffer.getFlatSize()).isEqualTo(0);
    }
  }

  @Test
  public void testLoadByteBufferWithDifferentFixedSize() {
    // Create a fixed-size TensorBuffer with size 2, and load a ByteBuffer with size 5.
    int[] tensorBufferShape = new int[] {2};
    TensorBuffer tensorBuffer = TensorBuffer.createFixedSize(tensorBufferShape, DataType.FLOAT32);
    Assert.assertThrows(
        IllegalArgumentException.class,
        () -> tensorBuffer.loadBuffer(FLOAT_BYTE_BUFFER1, ARRAY1_SHAPE));
  }

  @Test
  public void testLoadByteBufferWithMisMatchDataType() {
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
    int[] wrongShape = new int[] {1};
    // Size of INT_BYTE_BUFFER is 8 bytes. It does not match the specified shape.
    Assert.assertThrows(
        IllegalArgumentException.class,
        () -> tensorBuffer.loadBuffer(INT_BYTE_BUFFER2, wrongShape));
  }

  @Test
  public void testLoadByteBufferForTensorBufferFloat() {
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
    tensorBuffer.loadBuffer(FLOAT_BYTE_BUFFER1, ARRAY1_SHAPE);
    assertThat(tensorBuffer.getFloatArray()).isEqualTo(FLOAT_ARRAY1);
    assertThat(tensorBuffer.getShape()).isEqualTo(ARRAY1_SHAPE);
  }

  @Test
  public void testLoadByteBufferForTensorBufferUint8() {
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.UINT8);
    tensorBuffer.loadBuffer(INT_BYTE_BUFFER2, ARRAY2_SHAPE);
    assertThat(tensorBuffer.getIntArray()).isEqualTo(INT_ARRAY2);
    assertThat(tensorBuffer.getShape()).isEqualTo(ARRAY2_SHAPE);
  }

  @Test
  public void testGetFloatValueWithInvalidIndex() {
    float[] arrayWithSixElements = FLOAT_ARRAY1;
    int[] shapeOfArrayWithSixElements = ARRAY1_SHAPE;
    int[] invalidIndexes = {-1, 7};
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      tensorBuffer.loadArray(arrayWithSixElements, shapeOfArrayWithSixElements);
      for (int invalidIndex : invalidIndexes) {
        Assert.assertThrows(
            IndexOutOfBoundsException.class, () -> tensorBuffer.getFloatValue(invalidIndex));
      }
    }
  }

  @Test
  public void testGetFloatValueFromScalarWithInvalidIndex() {
    int[] shape = new int[] {};
    float[] arr = new float[] {10.0f};
    int[] invalidIndexes =
        new int[] {-1, 1}; // -1 is negative, and 1 is not smaller than the flatsize.
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      tensorBuffer.loadArray(arr, shape);
      for (int invalidIndex : invalidIndexes) {
        Assert.assertThrows(
            IndexOutOfBoundsException.class, () -> tensorBuffer.getFloatValue(invalidIndex));
      }
    }
  }

  @Test
  public void testGetIntValueWithInvalidIndex() {
    float[] arrayWithSixElements = FLOAT_ARRAY1;
    int[] shapeOfArrayWithSixElements = ARRAY1_SHAPE;
    int[] invalidIndexes = {-1, 7};
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      tensorBuffer.loadArray(arrayWithSixElements, shapeOfArrayWithSixElements);
      for (int invalidIndex : invalidIndexes) {
        Assert.assertThrows(
            IndexOutOfBoundsException.class, () -> tensorBuffer.getIntValue(invalidIndex));
      }
    }
  }

  @Test
  public void testGetIntValueFromScalarWithInvalidIndex() {
    int[] shape = new int[] {};
    float[] arr = new float[] {10.0f};
    int[] invalidIndexes =
        new int[] {-1, 1}; // -1 is negative, and 1 is not smaller than the flatsize.
    for (DataType dataType : ArrayTestRunner.getBufferTypeList()) {
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(dataType);
      tensorBuffer.loadArray(arr, shape);
      for (int invalidIndex : invalidIndexes) {
        Assert.assertThrows(
            IndexOutOfBoundsException.class, () -> tensorBuffer.getIntValue(invalidIndex));
      }
    }
  }

  @Test
  public void testLoadByteBufferSliceForTensorBufferFloat() {
    TensorBuffer original = TensorBuffer.createDynamic(DataType.FLOAT32);
    original.loadArray(new float[] {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, new int[] {6});
    ByteBuffer buffer = original.getBuffer();
    // Slice original buffer to 3 sub-buffer, each of which has 2 element
    int numBuffers = 3;
    int numElements = 2;
    int subArrayLength = numElements * original.getTypeSize();
    TensorBuffer tensorSlice = TensorBuffer.createDynamic(original.getDataType());
    for (int i = 0; i < numBuffers; i++) {
      buffer.position(i * subArrayLength);
      ByteBuffer subBuffer = buffer.slice();
      // ByteBuffer.slice doesn't keep order.
      subBuffer.order(buffer.order()).limit(subArrayLength);
      tensorSlice.loadBuffer(subBuffer, new int[] {numElements});
      float[] arraySlice = tensorSlice.getFloatArray();
      assertThat(arraySlice.length).isEqualTo(numElements);
      assertThat(arraySlice[0]).isEqualTo(i * numElements + 1);
      assertThat(arraySlice[1]).isEqualTo(i * numElements + 2);
    }
  }

  @Test
  public void testLoadByteBufferSliceForTensorBufferUInt8() {
    TensorBuffer original = TensorBuffer.createDynamic(DataType.UINT8);
    original.loadArray(new int[] {1, 2, 3, 4, 5, 6}, new int[] {6});
    ByteBuffer buffer = original.getBuffer();
    // Slice original buffer to 3 sub-buffer, each of which has 2 element
    int numBuffers = 3;
    int numElements = 2;
    int subArrayLength = numElements * original.getTypeSize();
    TensorBuffer tensorSlice = TensorBuffer.createDynamic(original.getDataType());
    for (int i = 0; i < numBuffers; i++) {
      buffer.position(i * subArrayLength);
      ByteBuffer subBuffer = buffer.slice();
      // ByteBuffer.slice doesn't keep order.
      subBuffer.order(buffer.order()).limit(subArrayLength);
      tensorSlice.loadBuffer(subBuffer, new int[] {numElements});
      int[] arraySlice = tensorSlice.getIntArray();
      assertThat(arraySlice.length).isEqualTo(numElements);
      assertThat(arraySlice[0]).isEqualTo(i * numElements + 1);
      assertThat(arraySlice[1]).isEqualTo(i * numElements + 2);
    }
  }

  @Test
  public void getShapeFailsAfterByteBufferChanged() {
    TensorBuffer tensorBuffer = TensorBuffer.createFixedSize(new int[] {3, 2}, DataType.FLOAT32);
    ByteBuffer byteBuffer = tensorBuffer.getBuffer();
    byteBuffer.limit(5);

    IllegalStateException exception =
        assertThrows(IllegalStateException.class, tensorBuffer::getShape);
    assertThat(exception)
        .hasMessageThat()
        .contains(
            "The size of underlying ByteBuffer (5) and the shape ([3, 2]) do not match. The"
                + " ByteBuffer may have been changed.");
  }

  @Test
  public void getFlatSizeFailsAfterByteBufferChanged() {
    TensorBuffer tensorBuffer = TensorBuffer.createFixedSize(new int[] {3, 2}, DataType.FLOAT32);
    ByteBuffer byteBuffer = tensorBuffer.getBuffer();
    byteBuffer.limit(5);

    IllegalStateException exception =
        assertThrows(IllegalStateException.class, tensorBuffer::getFlatSize);
    assertThat(exception)
        .hasMessageThat()
        .contains(
            "The size of underlying ByteBuffer (5) and the shape ([3, 2]) do not match. The"
                + " ByteBuffer may have been changed.");
  }

  @Test
  public void loadReadOnlyBuffersCopiesOnWrite() {
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.UINT8);
    ByteBuffer originalByteBuffer = ByteBuffer.allocateDirect(1);
    originalByteBuffer.put(new byte[]{99});
    originalByteBuffer.rewind();
    ByteBuffer readOnlyByteBuffer = originalByteBuffer.asReadOnlyBuffer();

    tensorBuffer.loadBuffer(readOnlyByteBuffer, new int[]{1});
    assertThat(tensorBuffer.getBuffer()).isSameInstanceAs(readOnlyByteBuffer);

    tensorBuffer.loadArray(new int[]{42});
    assertThat(tensorBuffer.getBuffer()).isNotSameInstanceAs(readOnlyByteBuffer);
    assertThat(tensorBuffer.getBuffer().get(0)).isEqualTo(42);  // updated
    assertThat(originalByteBuffer.get(0)).isEqualTo(99);  // original one not changed
  }
}
