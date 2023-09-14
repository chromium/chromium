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

package org.tensorflow.lite.support.image;

import static com.google.common.truth.Truth.assertThat;

import android.graphics.RectF;
import java.util.List;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.image.BoundingBoxUtil.CoordinateType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests of {@link BoundingBoxUtil}. */
@RunWith(RobolectricTestRunner.class)
public class BoundingBoxUtilTest {

  private TensorBuffer tensorBuffer;

  @Before
  public void setUp() {
    // 2 bounding boxes with additional batch dimension.
    tensorBuffer = TensorBuffer.createFixedSize(new int[] {1, 2, 4}, DataType.FLOAT32);
  }

  @Test
  public void convertDefaultRatioBoundaries() {
    tensorBuffer.loadArray(new float[] {0.25f, 0.2f, 0.75f, 0.8f, 0.5f, 0.0f, 1.0f, 1.0f});

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {0, 1, 2, 3},
            -1,
            BoundingBoxUtil.Type.BOUNDARIES,
            CoordinateType.RATIO,
            500,
            400);

    assertThat(boxList).hasSize(2);
    assertThat(boxList.get(0)).isEqualTo(new RectF(100, 100, 300, 400));
    assertThat(boxList.get(1)).isEqualTo(new RectF(200, 0, 400, 500));
  }

  @Test
  public void convertComplexTensor() {
    tensorBuffer = TensorBuffer.createFixedSize(new int[] {3, 4, 2}, DataType.FLOAT32);
    tensorBuffer.loadArray(
        new float[] {
          // sub tensor 0
          0, 1, 10, 11, 20, 21, 30, 31,
          // sub tensor 1
          100, 101, 110, 111, 120, 121, 130, 131,
          // sub tensor 2
          200, 201, 210, 211, 220, 221, 230, 231
        });

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {0, 1, 2, 3},
            1,
            BoundingBoxUtil.Type.BOUNDARIES,
            CoordinateType.PIXEL,
            0,
            0);

    assertThat(boxList).hasSize(6);
    assertThat(boxList.get(0)).isEqualTo(new RectF(0, 10, 20, 30));
    assertThat(boxList.get(1)).isEqualTo(new RectF(1, 11, 21, 31));
    assertThat(boxList.get(2)).isEqualTo(new RectF(100, 110, 120, 130));
    assertThat(boxList.get(3)).isEqualTo(new RectF(101, 111, 121, 131));
  }

  @Test
  public void convertIndexedRatioBoundaries() {
    tensorBuffer.loadArray(new float[] {0.25f, 0.2f, 0.75f, 0.8f, 0.5f, 0.0f, 1.0f, 1.0f});

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {1, 0, 3, 2},
            -1,
            BoundingBoxUtil.Type.BOUNDARIES,
            CoordinateType.RATIO,
            500,
            400);

    assertThat(boxList).hasSize(2);
    assertThat(boxList.get(0)).isEqualTo(new RectF(80, 125, 320, 375));
    assertThat(boxList.get(1)).isEqualTo(new RectF(0, 250, 400, 500));
  }

  @Test
  public void convertPixelBoundaries() {
    tensorBuffer.loadArray(new float[] {100, 100, 300, 400, 200, 0, 400, 500});

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {0, 1, 2, 3},
            -1,
            BoundingBoxUtil.Type.BOUNDARIES,
            CoordinateType.PIXEL,
            500,
            400);

    assertThat(boxList)
        .containsExactly(new RectF(100, 100, 300, 400), new RectF(200, 0, 400, 500))
        .inOrder();
  }

  @Test
  public void convertRatioUpperLeft() {
    tensorBuffer.loadArray(new float[] {0.25f, 0.2f, 0.5f, 0.6f, 0.5f, 0.0f, 0.5f, 1.0f});

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {0, 1, 2, 3},
            -1,
            BoundingBoxUtil.Type.UPPER_LEFT,
            CoordinateType.RATIO,
            500,
            400);

    assertThat(boxList).hasSize(2);
    assertThat(boxList)
        .containsExactly(new RectF(100, 100, 300, 400), new RectF(200, 0, 400, 500))
        .inOrder();
  }

  @Test
  public void convertPixelUpperLeft() {
    tensorBuffer.loadArray(new float[] {100, 100, 200, 300, 200, 0, 200, 500});

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {0, 1, 2, 3},
            -1,
            BoundingBoxUtil.Type.UPPER_LEFT,
            CoordinateType.PIXEL,
            500,
            400);

    assertThat(boxList)
        .containsExactly(new RectF(100, 100, 300, 400), new RectF(200, 0, 400, 500))
        .inOrder();
  }

  @Test
  public void convertRatioCenter() {
    tensorBuffer.loadArray(new float[] {0.5f, 0.5f, 0.5f, 0.6f, 0.75f, 0.5f, 0.5f, 1.0f});

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {0, 1, 2, 3},
            -1,
            BoundingBoxUtil.Type.CENTER,
            CoordinateType.RATIO,
            500,
            400);

    assertThat(boxList)
        .containsExactly(new RectF(100, 99.99999f, 300, 400), new RectF(200, 0, 400, 500))
        .inOrder();
  }

  @Test
  public void convertPixelCenter() {
    tensorBuffer.loadArray(new float[] {200, 250, 200, 300, 300, 250, 200, 500});

    List<RectF> boxList =
        BoundingBoxUtil.convert(
            tensorBuffer,
            new int[] {0, 1, 2, 3},
            -1,
            BoundingBoxUtil.Type.CENTER,
            CoordinateType.PIXEL,
            500,
            400);

    assertThat(boxList)
        .containsExactly(new RectF(100, 100, 300, 400), new RectF(200, 0, 400, 500))
        .inOrder();
  }

  @Test
  public void convertTensorWithUnexpectedShapeShouldThrow() {
    TensorBuffer badShapeTensor = TensorBuffer.createFixedSize(new int[] {1, 5}, DataType.FLOAT32);

    Assert.assertThrows(
        IllegalArgumentException.class,
        () ->
            BoundingBoxUtil.convert(
                badShapeTensor,
                new int[] {0, 1, 2, 3},
                -1,
                BoundingBoxUtil.Type.BOUNDARIES,
                CoordinateType.RATIO,
                300,
                400));
  }

  @Test
  public void convertIntTensorShouldThrow() {
    TensorBuffer badTypeTensor = TensorBuffer.createFixedSize(new int[] {1, 4}, DataType.UINT8);

    Assert.assertThrows(
        IllegalArgumentException.class,
        () ->
            BoundingBoxUtil.convert(
                badTypeTensor,
                new int[] {0, 1, 2, 3},
                -1,
                BoundingBoxUtil.Type.BOUNDARIES,
                CoordinateType.RATIO,
                300,
                400));
  }
}
