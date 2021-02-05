# coding=utf-8
# Copyright 2021 TF.Text Authors.
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

"""Tokenizer for strings based on change in unicode script codes."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.eager import monitoring
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import string_ops
from tensorflow.python.ops.ragged import ragged_conversion_ops
from tensorflow.python.ops.ragged import ragged_string_ops
from tensorflow.python.ops.ragged import ragged_tensor
from tensorflow.python.ops.ragged.ragged_tensor import RaggedTensor
from tensorflow_text.python.ops.tokenization import TokenizerWithOffsets

# pylint: disable=g-bad-import-order
from tensorflow.python.framework import load_library
from tensorflow.python.platform import resource_loader
gen_unicode_script_tokenizer = load_library.load_op_library(resource_loader.get_path_to_datafile('_unicode_script_tokenizer.so'))

_tf_text_unicode_script_tokenizer_create_counter = monitoring.Counter(
    "/nlx/api/python/unicode_script_tokenizer_create_counter",
    "Counter for number of UnicodeScriptTokenizers created in Python.")


class UnicodeScriptTokenizer(TokenizerWithOffsets):
  """Tokenizes a tensor of UTF-8 strings on Unicode script boundaries."""

  def __init__(self, keep_whitespace=False):
    """Initializes a new instance.

    Args:
      keep_whitespace: A boolean that specifices whether to emit whitespace
          tokens (default `False`).
    """
    super(UnicodeScriptTokenizer, self).__init__()
    _tf_text_unicode_script_tokenizer_create_counter.get_cell().increase_by(1)
    self._keep_whitespace = keep_whitespace

  def tokenize(self, input):  # pylint: disable=redefined-builtin
    """Tokenizes a tensor of UTF-8 strings on Unicode script boundaries.

    The strings are split when successive tokens change their Unicode script
    or change being whitespace or not. The script codes used correspond to
    International Components for Unicode (ICU) UScriptCode values. See:
    http://icu-project.org/apiref/icu4c/uscript_8h.html

    ICU-defined whitespace characters are dropped, unless the `keep_whitespace`
    option was specified at construction time.

    Args:
      input: A `RaggedTensor`or `Tensor` of UTF-8 strings with any shape.

    Returns:
      A `RaggedTensor` of tokenized text. The returned shape is the shape of the
      input tensor with an added ragged dimension for tokens of each string.
    """
    (tokens, _, _) = self.tokenize_with_offsets(input)
    return tokens

  def tokenize_with_offsets(self, input):  # pylint: disable=redefined-builtin
    """Tokenizes a tensor of UTF-8 strings on Unicode script boundaries.

    The strings are split when a change in the Unicode script is detected
    between sequential tokens. The script codes used correspond to International
    Components for Unicode (ICU) UScriptCode values. See:
    http://icu-project.org/apiref/icu4c/uscript_8h.html

    ICU defined whitespace characters are dropped, unless the keep_whitespace
    option was specified at construction time.

    Args:
      input: A `RaggedTensor`or `Tensor` of UTF-8 strings with any shape.

    Returns:
      A tuple `(tokens, start_offsets, end_offsets)` where:

        * `tokens`: A `RaggedTensor` of tokenized text.
        * `start_offsets`: A `RaggedTensor` of the tokens' starting byte offset.
        * `end_offsets`: A `RaggedTensor` of the tokens' ending byte offset.
    """
    name = None
    with ops.name_scope(name, "UnicodeScriptTokenize", [input]):
      input_tensor = ragged_tensor.convert_to_tensor_or_ragged_tensor(input)
      if input_tensor.shape.ndims is None:
        raise ValueError("Rank of input_tensor must be statically known.")
      if ragged_tensor.is_ragged(input_tensor):
        if input_tensor.flat_values.shape.ndims > 1:
          # If the flat_values of our ragged tensor is multi-dimensional, we can
          # process it separately and our output will have the same nested
          # splits as our input.
          (tokens, starts, ends) = self.tokenize_with_offsets(
              input_tensor.flat_values)
          return (input_tensor.with_flat_values(tokens),
                  input_tensor.with_flat_values(starts),
                  input_tensor.with_flat_values(ends))
        else:
          # Recursively process the values of the ragged tensor.
          (tokens, starts, ends) = self.tokenize_with_offsets(
              input_tensor.values)
          return (input_tensor.with_values(tokens),
                  input_tensor.with_values(starts),
                  input_tensor.with_values(ends))
      else:
        if input_tensor.shape.ndims > 1:
          # Convert the input tensor to ragged and process it.
          return self.tokenize_with_offsets(
              ragged_conversion_ops.from_tensor(input_tensor))
        elif input_tensor.shape.ndims == 0:
          (tokens, starts, ends) = self.tokenize_with_offsets(
              array_ops.stack([input_tensor]))
          return tokens.values, starts.values, ends.values
        else:
          # Our rank 1 tensor is the correct shape, so we can process it as
          # normal
          return self._tokenize_with_offsets_encode_decode_wrapper(input_tensor)

  def _tokenize_with_offsets_encode_decode_wrapper(self, input_tensor):
    """Tokenizes a tensor of UTF-8 strings with rank of 1.

    Args:
      input_tensor: The single dimensional Tensor to tokenize.

    Returns:
      Tuple of RaggedTensors of tokenized text and byte offsets, with shapes
      [num_strings, (num_tokens or num_offsets)].
    """
    # Decode the strings and get byte offsets
    (codepoints, byte_start_offsets) = (
        ragged_string_ops.unicode_decode_with_offsets(input_tensor, "UTF-8"))
    byte_end_offsets = array_ops.concat([
        byte_start_offsets[:, 1:],
        math_ops.cast(
            array_ops.expand_dims(string_ops.string_length(input_tensor), 1),
            dtypes.int64)
    ], 1)

    # Tokenize
    (codepoint_tokens, codepoint_start_offsets, codepoint_end_offsets) = (
        self._tokenize_codepoints_with_offsets(codepoints))

    # Encode the codepoints and translate the codepoint offsets to byte offsets.
    return (ragged_string_ops.unicode_encode(codepoint_tokens, "UTF-8"),
            array_ops.batch_gather(byte_start_offsets, codepoint_start_offsets),
            array_ops.batch_gather(
                byte_end_offsets,
                math_ops.subtract(codepoint_end_offsets, [1])))

  def _tokenize_codepoints_with_offsets(self, codepoints_tensor):
    """Tokenizes a tensor of codepoints with rank of 1.

    Args:
      codepoints_tensor: Single-dimension Tensor of codepoints to tokenize.

    Returns:
      Tuple of tokenized codepoints with offsets relative to the codepoints have
      a shape of [num_strings, (num_tokens or num_offsets)].
    """
    (output_values, output_values_inner_splits, output_offset_starts,
     output_offset_ends, output_outer_splits) = (
         gen_unicode_script_tokenizer.unicode_script_tokenize_with_offsets(
             input_values=codepoints_tensor.flat_values,
             input_splits=codepoints_tensor.row_splits,
             keep_whitespace=self._keep_whitespace))
    codepoint_tokens = RaggedTensor.from_nested_row_splits(
        flat_values=output_values,
        nested_row_splits=[output_outer_splits, output_values_inner_splits])
    codepoint_offset_starts = RaggedTensor.from_nested_row_splits(
        flat_values=output_offset_starts,
        nested_row_splits=[output_outer_splits])
    codepoint_offset_ends = RaggedTensor.from_nested_row_splits(
        flat_values=output_offset_ends,
        nested_row_splits=[output_outer_splits])
    return (codepoint_tokens, codepoint_offset_starts, codepoint_offset_ends)
