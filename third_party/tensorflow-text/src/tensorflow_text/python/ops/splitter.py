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

"""Abstract base classes for all splitters."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import abc

from tensorflow.python.module import module


class Splitter(module.Module):
  """An abstract base class for splitting text."""

  __metaclass__ = abc.ABCMeta

  @abc.abstractmethod
  def split(self, input):  # pylint: disable=redefined-builtin
    """Splits the strings from the input tensor.

    Args:
      input: An N-dimensional UTF-8 string (or optionally integer) `Tensor` or
        `RaggedTensor`.

    Returns:
      An N+1-dimensional UTF-8 string or integer `Tensor` or `RaggedTensor`.
      For each string from the input tensor, the final, extra dimension contains
      the pieces that string was split into.
    """
    raise NotImplementedError("Abstract method")


class SplitterWithOffsets(Splitter):
  """An abstract base class for splitters that return offsets."""

  @abc.abstractmethod
  def split_with_offsets(self, input):  # pylint: disable=redefined-builtin
    """Splits the input tensor, returns the resulting pieces with offsets.

    Args:
      input: An N-dimensional UTF-8 string (or optionally integer) `Tensor` or
        `RaggedTensor`.

    Returns:
      A tuple `(pieces, start_offsets, end_offsets)` where:

        * `pieces` is an N+1-dimensional UTF-8 string or integer `Tensor` or
            `RaggedTensor`.
        * `start_offsets` is an N+1-dimensional integer `Tensor` or
            `RaggedTensor` containing the starting indices of each piece (byte
            indices for input strings).
        * `end_offsets` is an N+1-dimensional integer `Tensor` or
            `RaggedTensor` containing the exclusive ending indices of each piece
            (byte indices for input strings).
    """
    raise NotImplementedError("Abstract method")
