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
import static org.tensorflow.lite.DataType.FLOAT32;
import static org.tensorflow.lite.DataType.UINT8;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/**
 * Tests of {@link NormalizeOp}.
 */
@RunWith(RobolectricTestRunner.class)
public final class NormalizeOpTest {

  private static final float MEAN = 50;
  private static final float STDDEV = 50;
  private static final int NUM_ELEMENTS = 100;

  @Test
  public void testNormalizeIntBuffer() {
    int[] inputArr = new int[NUM_ELEMENTS];
    for (int i = 0; i < NUM_ELEMENTS; i++) {
      inputArr[i] = i;
    }
    TensorBuffer input = TensorBuffer.createDynamic(DataType.UINT8);
    input.loadArray(inputArr, new int[] {inputArr.length});
    NormalizeOp op = new NormalizeOp(MEAN, STDDEV);
    TensorBuffer output = op.apply(input);
    assertThat(output.getDataType()).isEqualTo(FLOAT32);
    float[] outputArr = output.getFloatArray();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
      assertThat(outputArr[i]).isEqualTo((inputArr[i] - MEAN) / STDDEV);
    }
  }

  @Test
  public void testNormalizeFloatBuffer() {
    float[] inputArr = new float[NUM_ELEMENTS];
    for (int i = 0; i < NUM_ELEMENTS; i++) {
      inputArr[i] = i;
    }
    TensorBuffer input = TensorBuffer.createDynamic(FLOAT32);
    input.loadArray(inputArr, new int[] {inputArr.length});
    NormalizeOp op = new NormalizeOp(MEAN, STDDEV);
    TensorBuffer output = op.apply(input);
    assertThat(output.getDataType()).isEqualTo(FLOAT32);
    float[] outputArr = output.getFloatArray();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
      assertThat(outputArr[i]).isEqualTo((inputArr[i] - MEAN) / STDDEV);
    }
  }

  @Test
  public void testZeroStddev() {
    Assert.assertThrows(IllegalArgumentException.class, () -> new NormalizeOp(1, 0));
  }

  @Test
  public void testIdentityShortcut() {
    TensorBuffer input = TensorBuffer.createFixedSize(new int[] {3, 3}, UINT8);
    NormalizeOp op = new NormalizeOp(0, 1);
    TensorBuffer output = op.apply(input);
    assertThat(output.getDataType()).isEqualTo(UINT8);
    assertThat(output).isSameInstanceAs(input);
  }

  @Test
  public void testNormalizeOp_zeroMeanAndZeroStddev() {
    TensorBuffer input = TensorBuffer.createFixedSize(new int[] {3, 3}, UINT8);
    NormalizeOp op = new NormalizeOp(0, 0);
    TensorBuffer output = op.apply(input);
    assertThat(output.getDataType()).isEqualTo(UINT8);
    assertThat(output).isSameInstanceAs(input);
  }

  @Test
  public void testNormalizeOp_zeroMeanAndInifityStddev() {
    TensorBuffer input = TensorBuffer.createFixedSize(new int[] {3, 3}, UINT8);
    NormalizeOp op = new NormalizeOp(0, Float.POSITIVE_INFINITY);
    TensorBuffer output = op.apply(input);
    assertThat(output.getDataType()).isEqualTo(UINT8);
    assertThat(output).isSameInstanceAs(input);
  }

  @Test
  public void testMultiChannelNormalize() {
    float[] inputArr = new float[NUM_ELEMENTS];
    for (int i = 0; i < NUM_ELEMENTS; i++) {
      inputArr[i] = i;
    }
    TensorBuffer input = TensorBuffer.createDynamic(FLOAT32);
    input.loadArray(inputArr, new int[] {20, 5});
    float[] means = new float[] {1, 2, 3, 4, 5};
    float[] stddevs = new float[] {6, 7, 8, 9, 10};
    NormalizeOp op = new NormalizeOp(means, stddevs);
    TensorBuffer output = op.apply(input);
    assertThat(output.getDataType()).isEqualTo(FLOAT32);
    float[] outputArr = output.getFloatArray();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
      assertThat(outputArr[i]).isEqualTo((i - means[i % 5]) / stddevs[i % 5]);
    }
  }

  @Test
  public void testMultiChannelShortcut() {
    TensorBuffer input = TensorBuffer.createFixedSize(new int[] {3, 3}, UINT8);
    NormalizeOp op = new NormalizeOp(new float[] {0, 0, 0}, new float[] {1, 1, 1});
    TensorBuffer output = op.apply(input);
    assertThat(output.getDataType()).isEqualTo(UINT8);
    assertThat(output).isSameInstanceAs(input);
  }

  @Test
  public void testMismatchedNumbersOfMeansAndStddevs() {
    Assert.assertThrows(
        IllegalArgumentException.class, () -> new NormalizeOp(new float[] {2, 3}, new float[] {1}));
  }

  @Test
  public void testMismatchedInputTensorChannelNum() {
    TensorBuffer input = TensorBuffer.createFixedSize(new int[] {3, 3}, UINT8);
    NormalizeOp op = new NormalizeOp(new float[] {0, 0}, new float[] {1, 2});
    Assert.assertThrows(IllegalArgumentException.class, () -> op.apply(input));
  }

  @Test
  public void testAnyChannelInvalidStddev() {
    Assert.assertThrows(
        IllegalArgumentException.class,
        () -> new NormalizeOp(new float[] {2, 3}, new float[] {1, 0}));
  }
}
