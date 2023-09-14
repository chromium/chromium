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

import android.graphics.Color;
import android.os.Build;
import androidx.annotation.RequiresApi;
import com.google.auto.value.AutoValue;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/** Represents a label associated with a color for display purposes. */
@AutoValue
@UsedByReflection("image_segmentation_jni.cc")
public abstract class ColoredLabel {

  /**
   * Creates a {@link ColoredLabel} object with an ARGB color int.
   *
   * @param label the label string, as provided in the label map packed in the TFLite Model
   *     Metadata.
   * @param displayName the display name of label, as configured through {@link
   *     ImageSegmenter.ImageSegmenterOptions.Builder#setDisplayNamesLocale}
   * @param argb the color components for the label in ARGB. See <a
   *     href="https://developer.android.com/reference/android/graphics/Color#color-ints">Android
   *     Color ints.</a> for more details.
   */
  @UsedByReflection("image_segmentation_jni.cc")
  public static ColoredLabel create(String label, String displayName, int argb) {
    return new AutoValue_ColoredLabel(label, displayName, argb);
  }

  /**
   * Creates a {@link ColoredLabel} object with a {@link android.graphics.Color} instance.
   *
   * @param label the label string, as provided in the label map packed in the TFLite Model
   *     Metadata.
   * @param displayName the display name of label, as configured through {@link
   *     ImageSegmenter.ImageSegmenterOptions.Builder#setDisplayNamesLocale}
   * @param color the color components for the label. The Color instatnce is supported on Android
   *     API level 26 and above. For API level lower than 26, use {@link #create(String, String,
   *     int)}. See <a
   *     href="https://developer.android.com/reference/android/graphics/Color#color-instances">Android
   *     Color instances.</a> for more details.
   */
  @RequiresApi(Build.VERSION_CODES.O)
  public static ColoredLabel create(String label, String displayName, Color color) {
    return new AutoValue_ColoredLabel(label, displayName, color.toArgb());
  }

  public abstract String getlabel();

  public abstract String getDisplayName();

  /**
   * Gets the ARGB int that represents the color.
   *
   * <p>See <a
   * href="https://developer.android.com/reference/android/graphics/Color#color-ints">Android Color
   * ints.</a> for more details.
   */
  public abstract int getArgb();

  /**
   * Gets the {@link android.graphics.Color} instance of the underlying color.
   *
   * <p>The Color instatnce is supported on Android API level 26 and above. For API level lower than
   * 26, use {@link #getArgb()}. See <a
   * href="https://developer.android.com/reference/android/graphics/Color#color-instances">Android
   * Color instances.</a> for more details.
   */
  @RequiresApi(Build.VERSION_CODES.O)
  public Color getColor() {
    return Color.valueOf(getArgb());
  }
}
