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

package org.tensorflow.lite.task.core.vision;

import android.graphics.Rect;
import com.google.auto.value.AutoValue;

/**
 * Options to configure the image processing pipeline, which operates before inference.
 *
 * <p>The Task Library Vision API performs image preprocessing on the input image over the region of
 * interest, so that it fits model requirements (e.g. upright 224x224 RGB) and populate the
 * corresponding input tensor. This is performed by (in this order):
 *
 * <ul>
 *   <li>cropping the frame buffer to the region of interest (which, in most cases, just covers the
 *       entire input image),
 *   <li>resizing it (with bilinear interpolation, aspect-ratio *not* preserved) to the dimensions
 *       of the model input tensor,
 *   <li>converting it to the colorspace of the input tensor (i.e. RGB, which is the only supported
 *       colorspace for now),
 *   <li>rotating it according to its {@link Orientation} so that inference is performed on an
 *       "upright" image.
 * </ul>
 *
 * <p>IMPORTANT: as a consequence of cropping occurring first, the provided region of interest is
 * expressed in the unrotated frame of reference coordinates system, i.e. in {@code [0,
 * TensorImage.getWidth()) x [0, TensorImage.getHeight())}, which are the dimensions of the
 * underlying image data before any orientation gets applied. If the region is out of these bounds,
 * the inference method, such as {@link
 * org.tensorflow.lite.task.vision.classifier.ImageClassifier#classify}, will return error.
 */
@AutoValue
public abstract class ImageProcessingOptions {

  /**
   * Orientation type that follows EXIF specification.
   *
   * <p>The name of each enum value defines the position of the 0th row and the 0th column of the
   * image content. See the <a href="http://jpegclub.org/exif_orientation.html">EXIF orientation
   * documentation</a> for details.
   */
  public enum Orientation {
    TOP_LEFT(0),
    TOP_RIGHT(1),
    BOTTOM_RIGHT(2),
    BOTTOM_LEFT(3),
    LEFT_TOP(4),
    RIGHT_TOP(5),
    RIGHT_BOTTOM(6),
    LEFT_BOTTOM(7);

    private final int value;

    Orientation(int value) {
      this.value = value;
    }

    public int getValue() {
      return value;
    }
  };

  private static final Rect defaultRoi = new Rect();
  private static final Orientation DEFAULT_ORIENTATION = Orientation.TOP_LEFT;

  public abstract Rect getRoi();

  public abstract Orientation getOrientation();

  public static Builder builder() {
    return new AutoValue_ImageProcessingOptions.Builder()
        .setRoi(defaultRoi)
        .setOrientation(DEFAULT_ORIENTATION);
  }

  /** Builder for {@link ImageProcessingOptions}. */
  @AutoValue.Builder
  public abstract static class Builder {

    /**
     * Sets the region of interest (ROI) of the image. Defaults to the entire image.
     *
     * <p>Cropping according to this region of interest is prepended to the pre-processing
     * operations.
     */
    public abstract Builder setRoi(Rect roi);

    /**
     * Sets the orientation of the image. Defaults to {@link Orientation#TOP_LEFT}.
     *
     * <p>Rotation will be applied accordingly so that inference is performed on an "upright" image.
     */
    public abstract Builder setOrientation(Orientation orientation);

    abstract Rect getRoi();

    abstract ImageProcessingOptions autoBuild();

    public ImageProcessingOptions build() {
      setRoi(new Rect(getRoi())); // Make a defensive copy, since Rect is mutable.
      return autoBuild();
    }
  }
}
