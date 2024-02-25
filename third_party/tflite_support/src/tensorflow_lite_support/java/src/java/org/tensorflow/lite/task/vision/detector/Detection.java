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

package org.tensorflow.lite.task.vision.detector;

import android.graphics.RectF;
import com.google.auto.value.AutoValue;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.tensorflow.lite.support.label.Category;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/** Represents one detected object in the results of a {@link ObjectDetector}. */
@AutoValue
@UsedByReflection("object_detection_jni.cc")
public abstract class Detection {

  @UsedByReflection("object_detection_jni.cc")
  public static Detection create(RectF boundingBox, List<Category> categories) {
    return new AutoValue_Detection(
        new RectF(boundingBox), Collections.unmodifiableList(new ArrayList<Category>(categories)));
  }

  public abstract RectF getBoundingBox();

  // Same reason for not using ImmutableList as stated in
  // {@link ObjectDetector#ObjectDetectorOptions#labelAllowList}.
  public abstract List<Category> getCategories();
}
