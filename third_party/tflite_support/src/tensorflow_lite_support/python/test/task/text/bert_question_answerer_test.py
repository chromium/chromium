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
"""Tests for bert_question_answerer."""

import enum

from absl.testing import parameterized

import tensorflow as tf
from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import qa_answers_pb2
from tensorflow_lite_support.python.task.text import bert_question_answerer
from tensorflow_lite_support.python.test import test_util

_BaseOptions = base_options_module.BaseOptions
_Pos = qa_answers_pb2.Pos
_QaAnswer = qa_answers_pb2.QaAnswer
_QuestionAnswererResult = qa_answers_pb2.QuestionAnswererResult
_BertQuestionAnswerer = bert_question_answerer.BertQuestionAnswerer
_BertQuestionAnswererOptions = bert_question_answerer.BertQuestionAnswererOptions

_INPUT_QUESTION = "What is a course of study called?"
_INPUT_CONTEXT = (
    "The role of teacher is often formal and ongoing, carried out at a school "
    "or other place of formal education. In many countries, a person who "
    "wishes to become a teacher must first obtain specified professional "
    "qualifications or credentials from a university or college. These "
    "professional qualifications may include the study of pedagogy, the "
    "science of teaching. Teachers, like other professionals, may have to "
    "continue their education after they qualify, a process known as "
    "continuing professional development. Teachers may use a lesson plan to "
    "facilitate student learning, providing a course of study which is called "
    "the curriculum.")

_MOBILE_BERT_MODEL = "mobilebert_with_metadata.tflite"
_EXPECTED_MOBILE_BERT_QA_RESULT = _QuestionAnswererResult(answers=[
    _QaAnswer(
        pos=_Pos(start=119, end=120, logit=18.815560), text="the curriculum."),
    _QaAnswer(
        pos=_Pos(start=120, end=120, logit=16.111582), text="curriculum."),
    _QaAnswer(
        pos=_Pos(start=119, end=121, logit=14.863710), text="the curriculum."),
    _QaAnswer(
        pos=_Pos(start=120, end=121, logit=12.159734), text="curriculum."),
    _QaAnswer(
        pos=_Pos(start=118, end=120, logit=10.609820),
        text="called the curriculum.")
])

_ALBERT_MODEL = "albert_with_metadata.tflite"
_EXPECTED_ALBERT_QA_RESULT = _QuestionAnswererResult(answers=[
    _QaAnswer(
        pos=_Pos(start=119, end=120, logit=19.000027), text="the curriculum."),
    _QaAnswer(
        pos=_Pos(start=120, end=120, logit=17.882782), text="curriculum."),
    _QaAnswer(
        pos=_Pos(start=119, end=121, logit=15.372071), text="the curriculum."),
    _QaAnswer(
        pos=_Pos(start=120, end=121, logit=14.254826), text="curriculum."),
    _QaAnswer(
        pos=_Pos(start=118, end=120, logit=12.718668),
        text="called the curriculum.")
])


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class BertQuestionAnswererTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.model_path = test_util.get_test_data_path(_MOBILE_BERT_MODEL)

  def test_create_from_file_succeeds_with_valid_model_path(self):
    # Creates with default option and valid model file successfully.
    answerer = _BertQuestionAnswerer.create_from_file(self.model_path)
    self.assertIsInstance(answerer, _BertQuestionAnswerer)

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _BertQuestionAnswererOptions(base_options=base_options)
    answerer = _BertQuestionAnswerer.create_from_options(options)
    self.assertIsInstance(answerer, _BertQuestionAnswerer)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      base_options = _BaseOptions(file_name="")
      options = _BertQuestionAnswererOptions(base_options=base_options)
      _BertQuestionAnswerer.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, "rb") as f:
      base_options = _BaseOptions(file_content=f.read())
      options = _BertQuestionAnswererOptions(base_options=base_options)
      answerer = _BertQuestionAnswerer.create_from_options(options)
      self.assertIsInstance(answerer, _BertQuestionAnswerer)

  @parameterized.parameters(
      (_MOBILE_BERT_MODEL, ModelFileType.FILE_NAME, _INPUT_CONTEXT,
       _INPUT_QUESTION, _EXPECTED_MOBILE_BERT_QA_RESULT),
      (_MOBILE_BERT_MODEL, ModelFileType.FILE_CONTENT, _INPUT_CONTEXT,
       _INPUT_QUESTION, _EXPECTED_MOBILE_BERT_QA_RESULT),
      (_ALBERT_MODEL, ModelFileType.FILE_NAME, _INPUT_CONTEXT, _INPUT_QUESTION,
       _EXPECTED_ALBERT_QA_RESULT),
      (_ALBERT_MODEL, ModelFileType.FILE_CONTENT, _INPUT_CONTEXT,
       _INPUT_QUESTION, _EXPECTED_ALBERT_QA_RESULT))
  def test_answer(self, model_name, model_file_type, context, question, answer):
    # Create question answerer.
    model_path = test_util.get_test_data_path(model_name)
    if model_file_type is ModelFileType.FILE_NAME:
      base_options = _BaseOptions(file_name=model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(model_path, "rb") as f:
        model_content = f.read()
      base_options = _BaseOptions(file_content=model_content)
    else:
      # Should never happen
      raise ValueError("model_file_type is invalid.")

    options = _BertQuestionAnswererOptions(base_options)
    question_answerer = _BertQuestionAnswerer.create_from_options(options)

    # Perform Bert Question Answering.
    text_result = question_answerer.answer(context, question)
    self.assertProtoEquals(
        text_result.to_pb2(), answer.to_pb2(), relative_tolerance=1e-4
    )


if __name__ == "__main__":
  tf.test.main()
