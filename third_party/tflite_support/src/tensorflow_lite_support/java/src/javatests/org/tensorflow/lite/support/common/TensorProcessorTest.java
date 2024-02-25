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

package org.tensorflow.lite.support.common;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.common.ops.NormalizeOp;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests for {@link TensorProcessor}. */
@RunWith(RobolectricTestRunner.class)
public final class TensorProcessorTest {

  private static final int EXAMPLE_NUM_FEATURES = 1000;
  private static final float MEAN = 127.5f;
  private static final float STDDEV = 127.5f;

  @Test
  public void testBuild() {
    TensorProcessor processor =
        new TensorProcessor.Builder().add(new NormalizeOp(MEAN, STDDEV)).build();
    assertThat(processor).isNotNull();
  }

  @Test
  public void testNormalize() {
    TensorBuffer input = createExampleTensorBuffer();
    TensorProcessor processor =
        new TensorProcessor.Builder().add(new NormalizeOp(MEAN, STDDEV)).build();
    TensorBuffer output = processor.process(input);

    float[] pixels = output.getFloatArray();
    assertThat(pixels.length).isEqualTo(EXAMPLE_NUM_FEATURES);
    for (float p : pixels) {
      assertThat(p).isAtLeast(-1);
      assertThat(p).isAtMost(1);
    }
  }

  @Test
  public void testMultipleNormalize() {
    TensorBuffer input = createExampleTensorBuffer();
    TensorProcessor processor =
        new TensorProcessor.Builder()
            .add(new NormalizeOp(MEAN, STDDEV)) // [0, 255] -> [-1, 1]
            .add(new NormalizeOp(-1, 2)) // [-1, 1] -> [0, 1]
            .build();
    TensorBuffer output = processor.process(input);

    float[] pixels = output.getFloatArray();
    assertThat(pixels.length).isEqualTo(EXAMPLE_NUM_FEATURES);
    for (float p : pixels) {
      assertThat(p).isAtLeast(0);
      assertThat(p).isAtMost(1);
    }
  }

  // Creates a TensorBuffer of size {1, 1000}, containing values in range [0, 255].
  private static TensorBuffer createExampleTensorBuffer() {
    TensorBuffer buffer = TensorBuffer.createDynamic(DataType.FLOAT32);
    int[] features = new int[EXAMPLE_NUM_FEATURES];
    for (int i = 0; i < EXAMPLE_NUM_FEATURES; i++) {
      features[i] = i % 256;
    }
    buffer.loadArray(features, new int[] {1, EXAMPLE_NUM_FEATURES});
    return buffer;
  }
}
