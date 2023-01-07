# Copyright 2022 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Bert Question Answerer task."""

import dataclasses

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import qa_answers_pb2
from tensorflow_lite_support.python.task.text.pybinds import _pywrap_bert_question_answerer

_CppBertQuestionAnswerer = _pywrap_bert_question_answerer.BertQuestionAnswerer
_BaseOptions = base_options_module.BaseOptions


@dataclasses.dataclass
class BertQuestionAnswererOptions:
  """Options for the Bert question answerer task."""
  base_options: _BaseOptions


class BertQuestionAnswerer(object):
  """Class that performs Bert question answering on text."""

  def __init__(self, options: BertQuestionAnswererOptions,
               cpp_bert_question_answerer: _CppBertQuestionAnswerer) -> None:
    """Initializes the `BertQuestionAnswerer` object."""
    # Creates the object of C++ QuestionAnswerer class.
    self._options = options
    self._question_answerer = cpp_bert_question_answerer

  @classmethod
  def create_from_file(cls, file_path: str) -> "BertQuestionAnswerer":
    """Creates the `BertQuestionAnswerer` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.

    Returns:
      `BertQuestionAnswerer` object that's created from the model file.
    Raises:
      ValueError: If failed to create `BertQuestionAnswerer` object from the
        provided file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = BertQuestionAnswererOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(
      cls, options: BertQuestionAnswererOptions) -> "BertQuestionAnswerer":
    """Creates the `BertQuestionAnswerer` object from the options.

    Args:
      options: Options for the Bert question answerer task.

    Returns:
      `BertQuestionAnswerer` object that's created from `options`.
    Raises:
      ValueError: If failed to create `BertQuestionAnswerer` object from
        `BertQuestionAnswererOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    question_answerer = _CppBertQuestionAnswerer.create_from_options(
        options.base_options.to_pb2())
    return cls(options, question_answerer)

  def answer(self, context: str,
             question: str) -> qa_answers_pb2.QuestionAnswererResult:
    """Answers question based on the context.

    Could be empty if no answer was
      found from the given context.

    Args:
      context: Context the question bases on.
      question: Question to ask.

    Returns:
      Question answerer result.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to calculate the embedding vector.
    """
    question_answerer_result = self._question_answerer.answer(context, question)
    return qa_answers_pb2.QuestionAnswererResult.create_from_pb2(
        question_answerer_result)

  @property
  def options(self) -> BertQuestionAnswererOptions:
    return self._options
