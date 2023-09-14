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

package org.tensorflow.lite.task.text.qa;

import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/**
 * Answers to {@link QuestionAnswerer}. Contains information about the answer and its relative
 * position information to the context.
 */
public class QaAnswer {
  public Pos pos;
  public String text;

  @UsedByReflection("bert_question_answerer_jni.cc")
  public QaAnswer(String text, Pos pos) {
    this.text = text;
    this.pos = pos;
  }

  public QaAnswer(String text, int start, int end, float logit) {
    this(text, new Pos(start, end, logit));
  }

  /**
   * Position information of the answer relative to context. It is sortable in descending order
   * based on logit.
   */
  public static class Pos implements Comparable<Pos> {
    public int start;
    public int end;
    public float logit;

    public Pos(int start, int end, float logit) {
      this.start = start;
      this.end = end;
      this.logit = logit;
    }

    @Override
    public int compareTo(Pos other) {
      return Float.compare(other.logit, this.logit);
    }
  }
}
