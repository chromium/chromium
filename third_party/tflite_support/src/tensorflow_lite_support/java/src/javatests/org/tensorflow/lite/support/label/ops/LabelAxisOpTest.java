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

package org.tensorflow.lite.support.label.ops;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.common.FileUtil;
import org.tensorflow.lite.support.label.TensorLabel;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests of {@link org.tensorflow.lite.support.label.ops.LabelAxisOp}. */
@RunWith(RobolectricTestRunner.class)
public final class LabelAxisOpTest {

  private final Context context = ApplicationProvider.getApplicationContext();
  private static final String LABEL_PATH = "flower_labels.txt";

  @Test
  public void testAddAxisLabelByStringList() {
    int numberLabel = 2;
    float[] inputArr = {0.7f, 0.3f};

    int[] shape = {numberLabel};
    TensorBuffer input = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    input.loadArray(inputArr, shape);

    List<String> labels = Arrays.asList("pos", "neg");
    LabelAxisOp op = new LabelAxisOp.Builder().addAxisLabel(0, labels).build();
    TensorLabel output = op.apply(input);
    Map<String, TensorBuffer> map = output.getMapWithTensorBuffer();

    assertThat(map).containsKey("pos");
    float[] array = map.get("pos").getFloatArray();
    assertThat(array).hasLength(1);
    assertThat(array[0]).isEqualTo(0.7f);

    assertThat(map).containsKey("neg");
    array = map.get("neg").getFloatArray();
    assertThat(array).hasLength(1);
    assertThat(array[0]).isEqualTo(0.3f);
  }

  @Test
  public void testAddAxisLabelWithMultiDimensionTensor() throws IOException {
    int numberLabel = 2;
    int numDim = 3;
    float[] inputArr = {0.5f, 0.1f, 0.3f, 0.2f, 0.2f, 0.1f};

    int[] shape = {1, numberLabel, numDim};
    TensorBuffer input = TensorBuffer.createFixedSize(shape, DataType.FLOAT32);
    input.loadArray(inputArr, shape);

    List<String> labels = Arrays.asList("pos", "neg");
    LabelAxisOp op = new LabelAxisOp.Builder().addAxisLabel(1, labels).build();

    TensorLabel output = op.apply(input);
    Map<String, TensorBuffer> map = output.getMapWithTensorBuffer();

    assertThat(map).containsKey("pos");
    float[] array = map.get("pos").getFloatArray();
    assertThat(array).hasLength(numDim);
    assertThat(array).isEqualTo(new float[] {0.5f, 0.1f, 0.3f});

    assertThat(map).containsKey("neg");
    array = map.get("neg").getFloatArray();
    assertThat(array).hasLength(numDim);
    assertThat(array).isEqualTo(new float[] {0.2f, 0.2f, 0.1f});
  }

  @Test
  public void testAddAxisLabelByFilePath() throws IOException {
    int numberLabel = 5;
    int[] inputArr = new int[numberLabel];
    for (int i = 0; i < numberLabel; i++) {
      inputArr[i] = i;
    }

    int[] shape = {numberLabel};
    TensorBuffer input = TensorBuffer.createFixedSize(shape, DataType.UINT8);
    input.loadArray(inputArr, shape);

    LabelAxisOp op = new LabelAxisOp.Builder().addAxisLabel(context, 0, LABEL_PATH).build();
    TensorLabel output = op.apply(input);
    Map<String, TensorBuffer> map = output.getMapWithTensorBuffer();

    List<String> labels = FileUtil.loadLabels(context, LABEL_PATH);
    for (int i = 0; i < numberLabel; i++) {
      String label = labels.get(i);

      assertThat(map).containsKey(label);

      int[] array = map.get(label).getIntArray();
      assertThat(array).hasLength(1);
      assertThat(array[0]).isEqualTo(inputArr[i]);
    }
  }
}
