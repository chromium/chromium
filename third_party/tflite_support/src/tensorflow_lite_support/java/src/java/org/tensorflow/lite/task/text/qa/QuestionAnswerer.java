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

import java.util.List;

/** API to answer questions based on context. */
public interface QuestionAnswerer {

  /**
   * Answers question based on context, and returns a list of possible {@link QaAnswer}s. Could be
   * empty if no answer was found from the given context.
   *
   * @param context context the question bases on
   * @param question question to ask
   * @return a list of possible answers in {@link QaAnswer}
   */
  List<QaAnswer> answer(String context, String question);
}
