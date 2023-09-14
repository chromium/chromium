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

package org.tensorflow.lite.support.label;

import static com.google.common.truth.Truth.assertThat;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests of {@link org.tensorflow.lite.support.label.TensorLabel}. */
@RunWith(RobolectricTestRunner.class)
public final class TensorLabelTest {
  @Test
  public void createTensorLabelWithNullAxisLabelsShouldFail() {
    int[] shape = {2};
    int[] arr = {1, 2};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, DataType.UINT8);
    buffer.loadArray(arr, shape);
    Map<Integer, List<String>> nullAxisLabels = null;

    Assert.assertThrows(NullPointerException.class, () -> new TensorLabel(nullAxisLabels, buffer));
  }

  @Test
  public void createTensorLabelWithNullTensorBufferShouldFail() {
    Map<Integer, List<String>> axisLabels = new HashMap<>();
    axisLabels.put(1, Arrays.asList("a", "b", "c", "d"));
    TensorBuffer nullTensorBuffer = null;

    Assert.assertThrows(
        NullPointerException.class, () -> new TensorLabel(axisLabels, nullTensorBuffer));
  }

  @Test
  public void createTensorLabelWithStringListShouldSuccess() {
    TensorBuffer buffer = TensorBuffer.createFixedSize(new int[] {1, 4, 3}, DataType.FLOAT32);

    TensorLabel tensorLabel = new TensorLabel(Arrays.asList("a", "b", "c", "d"), buffer);

    assertThat(tensorLabel.getMapWithTensorBuffer()).isNotNull();
    assertThat(tensorLabel.getMapWithTensorBuffer().keySet()).contains("c"); // randomly pick one
  }

  @Test
  public void createTensorLabelWithEmptyShapeShouldFail() {
    int[] shape = new int[] {};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    Map<Integer, List<String>> axisLabels = new HashMap<>();
    axisLabels.put(1, Arrays.asList("a", "b", "c", "d"));

    Assert.assertThrows(IllegalArgumentException.class, () -> new TensorLabel(axisLabels, buffer));
  }

  @Test
  public void createTensorLabelWithMismatchedAxisShouldFail() {
    int[] shape = {1, 4};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    Map<Integer, List<String>> axisLabels = new HashMap<>();
    axisLabels.put(0, Arrays.asList("a", "b", "c", "d"));

    Assert.assertThrows(IllegalArgumentException.class, () -> new TensorLabel(axisLabels, buffer));
  }

  @Test
  public void createTensorLabelWithMismatchedShapeShouldFail() {
    int[] shape = {1, 3};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    Map<Integer, List<String>> axisLabels = new HashMap<>();
    axisLabels.put(1, Arrays.asList("a", "b", "c", "d"));

    Assert.assertThrows(IllegalArgumentException.class, () -> new TensorLabel(axisLabels, buffer));
  }

  @Test
  public void getMapWithFloatBufferValuesShouldSuccess() {
    int numberLabel = 4;
    float[] inputArr = {0.5f, 0.2f, 0.2f, 0.1f};
    int[] shape = {1, numberLabel};
    TensorBuffer input = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    input.loadArray(inputArr, shape);
    Map<Integer, List<String>> axisLabels = new HashMap<>();
    int labelAxis = 1;
    axisLabels.put(labelAxis, Arrays.asList("a", "b", "c", "d"));

    TensorLabel tensorLabeled = new TensorLabel(axisLabels, input);
    Map<String, TensorBuffer> map = tensorLabeled.getMapWithTensorBuffer();

    for (int i = 0; i < numberLabel; i++) {
      String label = axisLabels.get(labelAxis).get(i);
      assertThat(map).containsKey(label);
      float[] array = map.get(label).getFloatArray();
      assertThat(array).hasLength(1);
      assertThat(array[0]).isEqualTo(inputArr[i]);
    }
  }

  @Test
  public void getMapWithIntBufferValuesShouldSuccess() {
    int numberLabel = 3;
    int[] inputArr = {1, 2, 0};
    int[] shape = {1, 1, numberLabel};
    TensorBuffer input = TensorBuffer.createFixedSize(shape, DataType.UINT8);
    input.loadArray(inputArr, shape);
    Map<Integer, List<String>> axisLabels = new HashMap<>();
    int labelAxis = 2;
    axisLabels.put(labelAxis, Arrays.asList("x", "y", "z"));

    TensorLabel tensorLabeled = new TensorLabel(axisLabels, input);
    Map<String, TensorBuffer> map = tensorLabeled.getMapWithTensorBuffer();

    for (int i = 0; i < numberLabel; i++) {
      String label = axisLabels.get(labelAxis).get(i);
      assertThat(map).containsKey(label);
      int[] array = map.get(label).getIntArray();
      assertThat(array).hasLength(1);
      assertThat(array[0]).isEqualTo(inputArr[i]);
    }
  }

  @Test
  public void getFloatMapShouldSuccess() {
    int[] shape = {1, 3};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    buffer.loadArray(new float[] {1.0f, 2.0f, 3.0f});

    TensorLabel tensorLabeled = new TensorLabel(Arrays.asList("a", "b", "c"), buffer);
    Map<String, Float> map = tensorLabeled.getMapWithFloatValue();

    assertThat(map).hasSize(3);
    assertThat(map).containsEntry("a", 1.0f);
    assertThat(map).containsEntry("b", 2.0f);
    assertThat(map).containsEntry("c", 3.0f);
  }

  @Test
  public void getMapFromMultiDimensionalTensorBufferShouldSuccess() {
    int numberLabel = 2;
    int numDim = 3;
    float[] inputArr = {0.5f, 0.1f, 0.3f, 0.2f, 0.2f, 0.1f};
    int[] shape = {numberLabel, numDim};
    TensorBuffer input = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    input.loadArray(inputArr, shape);
    Map<Integer, List<String>> axisLabels = new HashMap<>();
    int labelAxis = 0;
    axisLabels.put(labelAxis, Arrays.asList("pos", "neg"));

    TensorLabel tensorLabeled = new TensorLabel(axisLabels, input);
    Map<String, TensorBuffer> map = tensorLabeled.getMapWithTensorBuffer();

    for (int i = 0; i < numberLabel; i++) {
      String label = axisLabels.get(labelAxis).get(i);
      assertThat(map).containsKey(label);

      float[] array = map.get(label).getFloatArray();
      assertThat(array).hasLength(numDim);
      for (int j = 0; j < numDim; j++) {
        assertThat(array[j]).isEqualTo(inputArr[i * numDim + j]);
      }
    }
  }

  @Test
  public void getCategoryListShouldSuccess() {
    int[] shape = {1, 3};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    buffer.loadArray(new float[] {1.0f, 2.0f, 3.0f});

    TensorLabel tensorLabeled = new TensorLabel(Arrays.asList("a", "b", "c"), buffer);
    List<Category> categories = tensorLabeled.getCategoryList();

    assertThat(categories).hasSize(3);
    assertThat(categories)
        .containsExactly(new Category("a", 1.0f), new Category("b", 2.0f), new Category("c", 3.0f));
  }
}
