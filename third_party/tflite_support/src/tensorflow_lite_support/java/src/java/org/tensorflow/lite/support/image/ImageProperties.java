/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkState;

import com.google.auto.value.AutoValue;

/**
 * Represents the properties of an image object when being loaded to a {@link TensorImage}. See
 * {@link TensorImage#load}. {@link ImageProperties} currently is only used with {@link
 * org.tensorflow.lite.support.tensorbuffer.TensorBuffer}.
 */
@AutoValue
public abstract class ImageProperties {

  private static final int DEFAULT_HEIGHT = -1;
  private static final int DEFAULT_WIDTH = -1;

  public abstract int getHeight();

  public abstract int getWidth();

  public abstract ColorSpaceType getColorSpaceType();

  public static Builder builder() {
    return new AutoValue_ImageProperties.Builder()
        .setHeight(DEFAULT_HEIGHT)
        .setWidth(DEFAULT_WIDTH);
  }

  /**
   * Builder for {@link ImageProperties}. Different image objects may require different properties.
   * See the detais below:
   *
   * <ul>
   *   {@link org.tensorflow.lite.support.tensorbuffer.TensorBuffer}:
   *   <li>Mandatory proterties: height / width / colorSpaceType. The shape of the TensorBuffer
   *       object will not be used to determine image height and width.
   * </ul>
   */
  @AutoValue.Builder
  public abstract static class Builder {
    public abstract Builder setHeight(int height);

    public abstract Builder setWidth(int width);

    public abstract Builder setColorSpaceType(ColorSpaceType colorSpaceType);

    abstract ImageProperties autoBuild();

    public ImageProperties build() {
      ImageProperties properties = autoBuild();
      // If width or hight are not configured by the Builder, they will be -1.
      // Enforcing all properties to be populated (AutoValue will error out if objects, like
      // colorSpaceType, are not set up), since they are required for TensorBuffer images.
      // If in the future we have some image object types that only require a portion of these
      // properties, we can delay the check when TensorImage#load() is executed.
      checkState(properties.getHeight() >= 0, "Negative image height is not allowed.");
      checkState(properties.getWidth() >= 0, "Negative image width is not allowed.");
      return properties;
    }
  }
}
