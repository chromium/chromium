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

package org.tensorflow.lite.task.vision.segmenter;

import static org.tensorflow.lite.DataType.FLOAT32;
import static org.tensorflow.lite.DataType.UINT8;
import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkArgument;
import static org.tensorflow.lite.support.image.ColorSpaceType.GRAYSCALE;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/**
 * Output mask type. This allows specifying the type of post-processing to perform on the raw model
 * results.
 */
public enum OutputType {

  /**
   * Gives a single output mask where each pixel represents the class which the pixel in the
   * original image was predicted to belong to.
   */
  CATEGORY_MASK(0) {
    /**
     * {@inheritDoc}
     *
     * @throws IllegalArgumentException if more than one {@link TensorImage} are provided, or if the
     *     color space of the {@link TensorImage} is not {@link ColorSpaceType#GRAYSCALE}
     */
    @Override
    void assertMasksMatchColoredLabels(List<TensorImage> masks, List<ColoredLabel> coloredLabels) {
      checkArgument(
          masks.size() == 1,
          "CATRGORY_MASK only allows one TensorImage in the list, providing " + masks.size());

      TensorImage mask = masks.get(0);
      checkArgument(
          mask.getColorSpaceType() == GRAYSCALE,
          "CATRGORY_MASK only supports masks of ColorSpaceType, GRAYSCALE, providing "
              + mask.getColorSpaceType());
    }

    /**
     * {@inheritDoc}
     *
     * @throws IllegalArgumentException if more than one {@link ByteBuffer} are provided in the list
     */
    @Override
    List<TensorImage> createMasksFromBuffer(List<ByteBuffer> buffers, int[] maskShape) {
      checkArgument(
          buffers.size() == 1,
          "CATRGORY_MASK only allows one mask in the buffer list, providing " + buffers.size());

      List<TensorImage> masks = new ArrayList<>();
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(UINT8);
      tensorBuffer.loadBuffer(buffers.get(0), maskShape);
      TensorImage tensorImage = new TensorImage(UINT8);
      tensorImage.load(tensorBuffer, GRAYSCALE);
      masks.add(tensorImage);

      return masks;
    }
  },

  /**
   * Gives a list of output masks where, for each mask, each pixel represents the prediction
   * confidence, usually in the [0, 1] range.
   */
  CONFIDENCE_MASK(1) {
    /**
     * {@inheritDoc}
     *
     * @throws IllegalArgumentException if more the size of the masks list does not match the size
     *     of the coloredlabels list, or if the color space type of the any mask is not {@link
     *     ColorSpaceType#GRAYSCALE}
     */
    @Override
    void assertMasksMatchColoredLabels(List<TensorImage> masks, List<ColoredLabel> coloredLabels) {
      checkArgument(
          masks.size() == coloredLabels.size(),
          String.format(
              "When using CONFIDENCE_MASK, the number of masks (%d) should match the number of"
                  + " coloredLabels (%d).",
              masks.size(), coloredLabels.size()));

      for (TensorImage mask : masks) {
        checkArgument(
            mask.getColorSpaceType() == GRAYSCALE,
            "CONFIDENCE_MASK only supports masks of ColorSpaceType, GRAYSCALE, providing "
                + mask.getColorSpaceType());
      }
    }

    @Override
    List<TensorImage> createMasksFromBuffer(List<ByteBuffer> buffers, int[] maskShape) {
      List<TensorImage> masks = new ArrayList<>();
      for (ByteBuffer buffer : buffers) {
        TensorBuffer tensorBuffer = TensorBuffer.createDynamic(FLOAT32);
        tensorBuffer.loadBuffer(buffer, maskShape);
        TensorImage tensorImage = new TensorImage(FLOAT32);
        tensorImage.load(tensorBuffer, GRAYSCALE);
        masks.add(tensorImage);
      }
      return masks;
    }
  };

  public int getValue() {
    return value;
  }

  /**
   * Verifies that the given list of masks matches the list of colored labels.
   *
   * @throws IllegalArgumentException if {@code masks} and {@code coloredLabels} do not match the
   *     output type
   */
  abstract void assertMasksMatchColoredLabels(
      List<TensorImage> masks, List<ColoredLabel> coloredLabels);

  /** Creates the masks in {@link TensorImage} based on the data in {@link ByteBuffer}. */
  abstract List<TensorImage> createMasksFromBuffer(List<ByteBuffer> buffers, int[] maskShape);

  private final int value;

  private OutputType(int value) {
    this.value = value;
  }
}
