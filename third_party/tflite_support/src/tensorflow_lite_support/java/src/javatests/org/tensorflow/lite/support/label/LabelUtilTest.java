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

package org.tensorflow.lite.support.label;

import static com.google.common.truth.Truth.assertThat;

import java.util.Arrays;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests of {@link org.tensorflow.lite.support.label.LabelUtil}. */
@RunWith(RobolectricTestRunner.class)
public class LabelUtilTest {

  @Test
  public void mapIndexToStringsWithInvalidValues() {
    String[] labels = new String[] {"background", "apple", "banana", "cherry", "date"};
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.UINT8);
    tensorBuffer.loadArray(new int[] {0, 1, 2, 3, 2, 5}, new int[] {1, 6});
    List<String> categories = LabelUtil.mapValueToLabels(tensorBuffer, Arrays.asList(labels), 1);
    assertThat(categories.toArray())
        .isEqualTo(new String[] {"apple", "banana", "cherry", "date", "cherry", ""});
  }

  @Test
  public void mapFloatIndexShouldCast() {
    String[] labels = new String[] {"background", "apple", "banana", "cherry", "date"};
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
    tensorBuffer.loadArray(new float[] {-1.1f, -0.3f, 0.3f, 1.2f, 1.8f, 1}, new int[] {1, 6});
    List<String> categories = LabelUtil.mapValueToLabels(tensorBuffer, Arrays.asList(labels), 1);
    assertThat(categories.toArray())
        .isEqualTo(new String[] {"background", "apple", "apple", "banana", "banana", "banana"});
  }
}
