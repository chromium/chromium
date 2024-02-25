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
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.tensorflow.lite.support.label.Category;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/** The output domain, intent, and slot information for the {@link BertCluAnnotator}. */
// Based on third_party/tensorflow_lite_support/cc/task/text/proto/clu.proto.
@AutoValue
@UsedByReflection("bert_clu_annotator_jni.cc")
public abstract class CluResponse {
  @UsedByReflection("bert_clu_annotator_jni.cc")
  static CluResponse create(
      List<Category> domains,
      List<Category> intents,
      List<CategoricalSlot> categoricalSlots,
      List<MentionedSlot> mentionedSlots) {
    return new AutoValue_CluResponse(
        Collections.unmodifiableList(new ArrayList<Category>(domains)),
        Collections.unmodifiableList(new ArrayList<Category>(intents)),
        Collections.unmodifiableList(new ArrayList<CategoricalSlot>(categoricalSlots)),
        Collections.unmodifiableList(new ArrayList<MentionedSlot>(mentionedSlots)));
  }

  // Same reason for not using ImmutableList as stated in
  // {@link ImageClassifier#ImageClassifierOptions#labelAllowList}.
  @SuppressWarnings("AutoValueImmutableFields")
  public abstract List<Category> getDomains();

  @SuppressWarnings("AutoValueImmutableFields")
  public abstract List<Category> getIntents();

  @SuppressWarnings("AutoValueImmutableFields")
  public abstract List<CategoricalSlot> getCategoricalSlots();

  @SuppressWarnings("AutoValueImmutableFields")
  public abstract List<MentionedSlot> getMentionedSlots();

  /** Represents a categorical slot whose values are within a finite set. */
  // Based on CategoricalSlot in third_party/tensorflow_lite_support/cc/task/text/proto/clu.proto.
  @AutoValue
  @UsedByReflection("bert_clu_annotator_jni.cc")
  public abstract static class CategoricalSlot {
    @UsedByReflection("bert_clu_annotator_jni.cc")
    static CategoricalSlot create(String slot, Category prediction) {
      return new AutoValue_CluResponse_CategoricalSlot(slot, prediction);
    }

    public abstract String getSlot();

    public abstract Category getPrediction();
  }

  /** A single mention. */
  // Based on Mention in third_party/tensorflow_lite_support/cc/task/text/proto/clu.proto.
  @AutoValue
  @UsedByReflection("bert_clu_annotator_jni.cc")
  public abstract static class Mention {
    @UsedByReflection("bert_clu_annotator_jni.cc")
    static Mention create(String value, float score, int start, int end) {
      return new AutoValue_CluResponse_Mention(value, score, start, end);
    }

    public abstract String getValue();

    public abstract float getScore();

    public abstract int getStart();

    public abstract int getEnd();
  }

  /** Represents a mentioned slot whose values are open text extracted from the input text. */
  // Based on MentionedSlot in
  // third_party/tensorflow_lite_support/cc/task/text/proto/clu.proto.
  @AutoValue
  @UsedByReflection("bert_clu_annotator_jni.cc")
  public abstract static class MentionedSlot {
    @UsedByReflection("bert_clu_annotator_jni.cc")
    static MentionedSlot create(String slot, Mention mention) {
      return new AutoValue_CluResponse_MentionedSlot(slot, mention);
    }

    public abstract String getSlot();

    public abstract Mention getMention();
  }
}
