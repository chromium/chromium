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

package org.tensorflow.lite.task.text.bertclu;

import com.google.auto.value.AutoValue;
import java.util.List;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/** The input dialogue history for the {@link BertCluAnnotator}. */
// Based on third_party/tensorflow_lite_support/cc/task/text/proto/clu.proto.
@AutoValue
@UsedByReflection("bert_clu_annotator_jni.cc")
public abstract class CluRequest {
  public static CluRequest create(List<String> utterances) {
    return new AutoValue_CluRequest(utterances);
  }

  @SuppressWarnings("AutoValueImmutableFields")
  @UsedByReflection("bert_clu_annotator_jni.cc")
  public abstract List<String> getUtterances();
}
