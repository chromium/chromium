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
# ==============================================================================
"""TensorFlow Lite Task Library Text APIs.

This module provides interface to run TensorFlow Lite natural language
processing models.
"""

from tensorflow_lite_support.python.task.text import text_embedder
from tensorflow_lite_support.python.task.text import text_searcher
from tensorflow_lite_support.python.task.text import nl_classifier
from tensorflow_lite_support.python.task.text import bert_nl_classifier
from tensorflow_lite_support.python.task.text import bert_question_answerer
from tensorflow_lite_support.python.task.text import bert_clu_annotator

TextEmbedder = text_embedder.TextEmbedder
TextEmbedderOptions = text_embedder.TextEmbedderOptions
TextSearcher = text_searcher.TextSearcher
TextSearcherOptions = text_searcher.TextSearcherOptions
NLClassifier = nl_classifier.NLClassifier
NLClassifierOptions = nl_classifier.NLClassifierOptions
BertNLClassifier = bert_nl_classifier.BertNLClassifier
BertNLClassifierOptions = bert_nl_classifier.BertNLClassifierOptions
BertQuestionAnswerer = bert_question_answerer.BertQuestionAnswerer
BertQuestionAnswererOptions = bert_question_answerer.BertQuestionAnswererOptions
BertCluAnnotator = bert_clu_annotator.BertCluAnnotator
BertCluAnnotatorOptions = bert_clu_annotator.BertCluAnnotatorOptions

# Remove unnecessary modules to avoid duplication in API docs.
del text_embedder
del text_searcher
del nl_classifier
del bert_nl_classifier
del bert_question_answerer
del bert_clu_annotator
