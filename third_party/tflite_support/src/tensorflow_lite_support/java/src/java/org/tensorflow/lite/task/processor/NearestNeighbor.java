/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.task.processor;

import com.google.auto.value.AutoValue;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/** Represents the search result of a Searcher model. */
@AutoValue
@UsedByReflection("searcher_jni.cc")
public abstract class NearestNeighbor {

  @UsedByReflection("searcher_jni.cc")
  static NearestNeighbor create(byte[] metadataArray, float distance) {
    // Convert byte[] metadataArray to ByteBuffer which handles endianess better.
    //
    // Ideally, the API should accept a ByteBuffer instead of a byte[]. However, converting byte[]
    // to ByteBuffer in JNI will lead to unnecessarily complex code which involves 6 more reflection
    // calls. We can make this method package private, because users in general shouldn't need to
    // create NearestNeighbor instances, but only consume the objects return from Task Library. This
    // API will be used mostly for internal purpose.
    ByteBuffer metadata = ByteBuffer.wrap(metadataArray);
    metadata.order(ByteOrder.nativeOrder());
    return new AutoValue_NearestNeighbor(metadata, distance);
  }

  /**
   * Gets the user-defined metadata about the result. This could be a label, a unique ID, a
   * serialized proto of some sort, etc.
   *
   * <p><b>Do not mutate</b> the returned metadata.
   */
  public abstract ByteBuffer getMetadata();

  /** Gets the distance score indicating how confident the result is. Lower is better. */
  public abstract float getDistance();
}
