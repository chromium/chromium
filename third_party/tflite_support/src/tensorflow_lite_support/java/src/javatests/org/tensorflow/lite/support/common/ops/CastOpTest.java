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

package org.tensorflow.lite.support.common.ops;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests of {@link CastOp}. */
@RunWith(RobolectricTestRunner.class)
public final class CastOpTest {

  private static final float[] FLOAT_ARRAY = new float[] {1.1f, 3.3f, 5.5f, 7.7f, 9.9f};
  private static final float[] CASTED_FLOAT_ARRAY = new float[] {1.0f, 3.0f, 5.0f, 7.0f, 9.0f};
  private static final int[] INT_ARRAY = new int[] {1, 3, 5, 7, 9};
  private static final int[] SHAPE = new int[] {5};

  @Test
  public void castFloat32ToUint8ShouldSuccess() {
    TensorBuffer floatBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
    floatBuffer.loadArray(FLOAT_ARRAY, SHAPE);
    CastOp op = new CastOp(DataType.UINT8);
    TensorBuffer uint8Buffer = op.apply(floatBuffer);
    assertThat(uint8Buffer.getDataType()).isEqualTo(DataType.UINT8);
    assertThat(uint8Buffer.getIntArray()).isEqualTo(INT_ARRAY);
  }

  @Test
  public void castUint8ToFloat32ShouldSuccess() {
    TensorBuffer uint8Buffer = TensorBuffer.createDynamic(DataType.UINT8);
    uint8Buffer.loadArray(INT_ARRAY, SHAPE);
    CastOp op = new CastOp(DataType.FLOAT32);
    TensorBuffer floatBuffer = op.apply(uint8Buffer);
    assertThat(floatBuffer.getDataType()).isEqualTo(DataType.FLOAT32);
    assertThat(floatBuffer.getFloatArray()).isEqualTo(CASTED_FLOAT_ARRAY);
  }

  @Test
  public void castFloat32ToFloat32ShouldNotRecreate() {
    TensorBuffer floatBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
    floatBuffer.loadArray(FLOAT_ARRAY, SHAPE);
    CastOp op = new CastOp(DataType.FLOAT32);
    TensorBuffer newBuffer = op.apply(floatBuffer);
    assertThat(newBuffer.getDataType()).isEqualTo(DataType.FLOAT32);
    assertThat(newBuffer).isSameInstanceAs(floatBuffer);
  }

  @Test
  public void castUint8ToUint8ShouldNotRecreate() {
    TensorBuffer uint8Buffer = TensorBuffer.createDynamic(DataType.UINT8);
    uint8Buffer.loadArray(INT_ARRAY, SHAPE);
    CastOp op = new CastOp(DataType.UINT8);
    TensorBuffer newBuffer = op.apply(uint8Buffer);
    assertThat(newBuffer.getDataType()).isEqualTo(DataType.UINT8);
    assertThat(newBuffer).isSameInstanceAs(uint8Buffer);
  }

  @Test
  public void castToUnsupportedDataTypeShouldThrow() {
    for (DataType type : new DataType[] {DataType.INT32, DataType.INT64, DataType.STRING}) {
      Assert.assertThrows(IllegalArgumentException.class, () -> new CastOp(type));
    }
  }
}
