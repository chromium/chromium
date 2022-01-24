# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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

"""Wrapped TF.Text friendly to Tensorflow Lite conversion."""

import tensorflow as tf
import tensorflow_text as tf_text


class WhitespaceTokenizer(tf_text.Tokenizer):
  """TFLite friendly API for tensorflow_text.WhitspaceTokenizer.tokenize.

  The strings are split on ICU defined whitespace characters. These
  whitespace characters are dropped. See more details in
  https://github.com/tensorflow/text/blob/master/docs/api_docs/python/text/WhitespaceTokenizer.md

  Does not currently support tokenize_with_offsets().
  """

  def __init__(self):
    super(WhitespaceTokenizer, self).__init__()
    self._tokenizer = tf_text.WhitespaceTokenizer()

  def tokenize(self, input_tensor):
    """Tokenize input strings.

    Args:
      input_tensor: A `Tensor` of UTF-8 strings with rank 0, 1 or 2.

    Returns:
      A `RaggedTensor` of tokenized text. The returned shape is the shape of the
      input tensor with an added ragged dimension for tokens of each string.
    """

    @tf.function(experimental_implements='name: "tftext:WhitespaceTokenizer"')
    def func(input_tensor):
      return self._tokenizer.tokenize(input_tensor)

    return func(input_tensor)


def ngrams(data,
           width,
           axis=-1,
           reduction_type=None,
           string_separator=' ',
           name=None):
  """TFLite friendly API for tensorflow_text.ngrams.

  Creates a tensor of n-grams based data, a token tensor. See more details in
  https://github.com/tensorflow/text/blob/master/docs/api_docs/python/text/ngrams.md

  Args:
    data: The data to reduce.  Must be convertible into a tf.Tensor or a
      tf.RaggedTensor (in which case it will be deconstructed into its component
      tf.Tensors).
    width: The width of the ngram window. If there is not sufficient data to
      fill out the ngram window, the resulting ngram will be empty.
    axis: The axis to create ngrams along. Note that for string join reductions,
      only axis '-1' is supported; for other reductions, any positive or
      negative axis can be used. Should be a constant.
    reduction_type: A member of the Reduction enum. Should be a constant.
      Currently supports:
      * `Reduction.STRING_JOIN`: Join strings in the window. Note that axis must
        be -1 here.
    string_separator: The separator string used for `Reduction.STRING_JOIN`.
      Ignored otherwise. Must be a string constant, not a Tensor.
    name: The op name.

  Returns:
    A tensor of ngrams.  If `data` is a ragged tensor, this will be a ragged
    tensor.  Otherwise it will be a plain tensor.
  """

  if reduction_type is not tf_text.Reduction.STRING_JOIN:
    # TODO(b/162082752): Provide support for Reduction.SUM and Reduction.MEAN
    raise tf.errors.InvalidArgumentError(
        None, None, 'only Reduction.STRING_JOIN is currently supported')

  if reduction_type is tf_text.Reduction.STRING_JOIN and axis != -1:
    raise tf.errors.InvalidArgumentError(
        None, None, 'For Reduction.STRING_JOIN, axis must be -1')

  experimental_implements = [
      'name: "tftext:Ngrams"',
      'attr { key: "width" value { i: %d } }' % width,
      'attr { key: "axis" value { i: %d } }' % axis,
      'attr { key: "reduction_type" value { s: "STRING_JOIN" } }',
      'attr { key: "string_separator" value { s: "%s" } }' % string_separator,
  ]
  experimental_implements = ' '.join(experimental_implements)

  if isinstance(data, tf.RaggedTensor):

    # Since `data` can not be converted directly into a Tensor, we define
    # ragged_func() which takes a deconstructed tf.RaggedTensor
    # (one flat_values tensor and N row_splits tensors), pass it the
    # deconstructed version of `data`, and then immediately reconstruct it
    # within ragged_func().
    @tf.function(experimental_implements=experimental_implements)
    def ragged_func(values, *args):
      ragged_tensor = tf.RaggedTensor.from_nested_row_splits(
          flat_values=values, nested_row_splits=args)
      return tf_text.ngrams(ragged_tensor, width, axis, reduction_type,
                            string_separator, name)

    return ragged_func(data.flat_values, *data.nested_row_splits)

  @tf.function(experimental_implements=experimental_implements)
  def func(data):
    return tf_text.ngrams(data, width, axis, reduction_type, string_separator,
                          name)

  return func(data)
