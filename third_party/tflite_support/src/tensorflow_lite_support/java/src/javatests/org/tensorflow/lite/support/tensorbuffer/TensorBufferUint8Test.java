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

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;

/** Tests of {@link org.tensorflow.lite.support.tensorbuffer.TensorBufferUint8}. */
@RunWith(RobolectricTestRunner.class)
public final class TensorBufferUint8Test {
  @Test
  public void testCreateDynamic() {
    TensorBufferUint8 tensorBufferUint8 = new TensorBufferUint8();
    assertThat(tensorBufferUint8).isNotNull();
  }

  @Test
  public void testCreateFixedSize() {
    int[] shape = new int[] {1, 2, 3};
    TensorBufferUint8 tensorBufferUint8 = new TensorBufferUint8(shape);
    assertThat(tensorBufferUint8).isNotNull();
    assertThat(tensorBufferUint8.getFlatSize()).isEqualTo(6);
  }

  @Test
  public void testCreateFixedSizeWithScalarShape() {
    int[] shape = new int[] {};
    TensorBufferUint8 tensorBufferUint8 = new TensorBufferUint8(shape);
    assertThat(tensorBufferUint8).isNotNull();
    assertThat(tensorBufferUint8.getFlatSize()).isEqualTo(1);
  }

  @Test
  public void testCreateWithNullShape() {
    int[] shape = null;
    Assert.assertThrows(NullPointerException.class, () -> new TensorBufferUint8(shape));
  }

  @Test
  public void testCreateWithInvalidShape() {
    int[] shape = new int[] {1, -1, 2};
    Assert.assertThrows(IllegalArgumentException.class, () -> new TensorBufferUint8(shape));
  }

  @Test
  public void testCreateUsingShapeWithZero() {
    int[] shape = new int[] {1, 0, 2};
    TensorBufferUint8 tensorBufferUint8 = new TensorBufferUint8(shape);
    assertThat(tensorBufferUint8).isNotNull();
    assertThat(tensorBufferUint8.getFlatSize()).isEqualTo(0);
  }

  @Test
  public void testGetDataType() {
    TensorBufferUint8 tensorBufferUint8 = new TensorBufferUint8();
    assertThat(tensorBufferUint8.getDataType()).isEqualTo(DataType.UINT8);
  }
}
