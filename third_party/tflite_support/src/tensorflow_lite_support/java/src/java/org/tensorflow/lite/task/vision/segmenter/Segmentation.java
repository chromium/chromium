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

import com.google.auto.value.AutoValue;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.tensorflow.lite.support.image.TensorImage;

/** Represents the segmentation result of an {@link ImageSegmenter}. */
@AutoValue
public abstract class Segmentation {

  /**
   * Creates a {@link Segmentation} object.
   *
   * <p>{@link Segmentation} provides two types of outputs as indicated through {@link OutputType}:
   *
   * <p>{@link OutputType#CATEGORY_MASK}: the result contains a single category mask, which is a
   * grayscale {@link TensorImage} with shape (height, width), in row major order. The value of each
   * pixel in this mask represents the class to which the pixel in the mask belongs. The pixel
   * values are in 1:1 corresponding with the colored labels, i.e. a pixel with value {@code i} is
   * associated with {@code coloredLabels.get(i)}.
   *
   * <p>{@link OutputType#CONFIDENCE_MASK}: the result contains a list of confidence masks, which
   * are in 1:1 correspondance with the colored labels, i.e. {@link masks.get(i)} is associated with
   * {@code coloredLabels.get(i)}. Each confidence mask is a grayscale {@link TensorImage} with
   * shape (height, width), in row major order. The value of each pixel in these masks represents
   * the confidence score for this particular class.
   *
   * <p>IMPORTANT: segmentation masks are not direcly suited for display, in particular:<br>
   * \* they are relative to the unrotated input frame, i.e. *not* taking into account the {@code
   * Orientation} flag of the input FrameBuffer, <br>
   * \* their dimensions are intrinsic to the model, i.e. *not* dependent on the input FrameBuffer
   * dimensions.
   *
   * <p>Example of such post-processing, assuming: <br>
   * \* an input FrameBuffer with width=640, height=480, orientation=kLeftBottom (i.e. the image
   * will be rotated 90° clockwise during preprocessing to make it "upright"), <br>
   * \* a model outputting masks of size 224x224. <br>
   * In order to be directly displayable on top of the input image assumed to be displayed *with*
   * the {@code Orientation} flag taken into account (according to the <a
   * href="http://jpegclub.org/exif_orientation.html">EXIF specification</a>), the masks need to be:
   * re-scaled to 640 x 480, then rotated 90° clockwise.
   *
   * @throws IllegalArgumentException if {@code masks} and {@code coloredLabels} do not match the
   *     {@code outputType}
   */
  static Segmentation create(
      OutputType outputType, List<TensorImage> masks, List<ColoredLabel> coloredLabels) {
    outputType.assertMasksMatchColoredLabels(masks, coloredLabels);

    return new AutoValue_Segmentation(
        outputType,
        Collections.unmodifiableList(new ArrayList<TensorImage>(masks)),
        Collections.unmodifiableList(new ArrayList<ColoredLabel>(coloredLabels)));
  }

  public abstract OutputType getOutputType();

  // As an open source project, we've been trying avoiding depending on common java libraries,
  // such as Guava, because it may introduce conflicts with clients who also happen to use those
  // libraries. Therefore, instead of using ImmutableList here, we convert the List into
  // unmodifiableList in create() to make it less vulnerable.
  public abstract List<TensorImage> getMasks();

  public abstract List<ColoredLabel> getColoredLabels();
}
