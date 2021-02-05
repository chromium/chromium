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

"""Tests for ops to trim segments."""
from absl.testing import parameterized

from tensorflow.python.framework import constant_op
from tensorflow.python.framework import test_util
from tensorflow.python.ops.ragged import ragged_factory_ops
from tensorflow.python.platform import test
from tensorflow_text.python.ops import trimmer_ops


@test_util.run_all_in_graph_and_eager_modes
class TrimmerOpsTest(test.TestCase, parameterized.TestCase):

  @parameterized.parameters([
      # pyformat: disable
      dict(
          segments=[
              # segment 1
              [[1, 2, 3], [4, 5], [6]],
              # segment 2
              [[10], [20], [30, 40, 50]]
          ],
          expected=[
              # segment 1
              [[True, True, False], [True, False], [True]],
              # Segment 2
              [[False], [False], [True, True, False]]
          ],
          max_seq_length=[[2], [1], [3]],
      ),
      dict(
          segments=[
              # segment 1
              [[1, 2, 3], [4, 5], [6]],
              # segment 2
              [[10], [20], [30, 40, 50]]
          ],
          expected=[
              # segment 1
              [[True, True, False], [True, False], [True]],
              # Segment 2
              [[False], [False], [True, True, False]]
          ],
          max_seq_length=[2, 1, 3],
      ),
      dict(
          segments=[
              # first segment
              [[b"hello"], [b"name", b"is"],
               [b"what", b"time", b"is", b"it", b"?"]],
              # second segment
              [[b"whodis", b"?"], [b"bond", b",", b"james", b"bond"],
               [b"5:30", b"AM"]],
          ],
          max_seq_length=2,
          expected=[
              # first segment
              [[True], [True, True], [True, True, False, False, False]],
              # second segment
              [[True, False], [False, False, False, False], [False, False]],
          ],
      ),
      dict(
          descr="Test when segments are rank 3 RaggedTensors",
          segments=[
              # first segment
              [[[b"hello"], [b"there"]], [[b"name", b"is"]],
               [[b"what", b"time"], [b"is"], [b"it"], [b"?"]]],
              # second segment
              [[[b"whodis"], [b"?"]], [[b"bond"], [b","], [b"james"],
                                       [b"bond"]], [[b"5:30"], [b"AM"]]],
          ],
          max_seq_length=2,
          expected=[[[[True], [True]], [[True, True]],
                     [[True, True], [False], [False], [False]]],
                    [[[False], [False]], [[False], [False], [False], [False]],
                     [[False], [False]]]],
      ),
      dict(
          descr="Test when segments are rank 3 RaggedTensors and axis = 1",
          segments=[
              # first segment
              [[[b"hello"], [b"there"]], [[b"name", b"is"]],
               [[b"what", b"time"], [b"is"], [b"it"], [b"?"]]],
              # second segment
              [[[b"whodis"], [b"?"]], [[b"bond"], [b","], [b"james"],
                                       [b"bond"]], [[b"5:30"], [b"AM"]]],
          ],
          axis=1,
          max_seq_length=2,
          expected=[
              # 1st segment
              [[True, True], [True], [True, True, False, False]],
              # 2nd segment
              [[False, False], [True, False, False, False], [False, False]],
          ],
      ),
      # pyformat: enable
  ])
  def testGenerateMask(self,
                       segments,
                       max_seq_length,
                       expected,
                       axis=-1,
                       descr=None):
    max_seq_length = constant_op.constant(max_seq_length)
    segments = [ragged_factory_ops.constant(i) for i in segments]
    expected = [ragged_factory_ops.constant(i) for i in expected]
    trimmer = trimmer_ops.WaterfallTrimmer(max_seq_length, axis=axis)
    actual = trimmer.generate_mask(segments)
    for expected_mask, actual_mask in zip(expected, actual):
      self.assertAllEqual(actual_mask, expected_mask)

  @parameterized.parameters([
      dict(
          segments=[
              # first segment
              [[b"hello", b"there"], [b"name", b"is"],
               [b"what", b"time", b"is", b"it", b"?"]],
              # second segment
              [[b"whodis", b"?"], [b"bond", b",", b"james", b"bond"],
               [b"5:30", b"AM"]],
          ],
          max_seq_length=[1, 3, 4],
          expected=[
              # Expected first segment has shape [3, (1, 2, 4)]
              [[b"hello"], [b"name", b"is"], [b"what", b"time", b"is", b"it"]],
              # Expected second segment has shape [3, (0, 1, 0)]
              [[], [b"bond"], []],
          ]),
      dict(
          descr="Test max sequence length across the batch",
          segments=[
              # first segment
              [[b"hello", b"there"], [b"name", b"is"],
               [b"what", b"time", b"is", b"it", b"?"]],
              # second segment
              [[b"whodis", b"?"], [b"bond", b",", b"james", b"bond"],
               [b"5:30", b"AM"]],
          ],
          max_seq_length=2,
          expected=[
              # Expected first segment has shape [3, (2, 2, 2)]
              [[b"hello", b"there"], [b"name", b"is"], [b"what", b"time"]],
              # Expected second segment has shape [3, (0, 0, 0)]
              [[], [], []],
          ],
      ),
      dict(
          descr="Test when segments are rank 3 RaggedTensors",
          segments=[
              # first segment
              [[[b"hello"], [b"there"]], [[b"name", b"is"]],
               [[b"what", b"time"], [b"is"], [b"it"], [b"?"]]],
              # second segment
              [[[b"whodis"], [b"?"]], [[b"bond"], [b","], [b"james"],
                                       [b"bond"]], [[b"5:30"], [b"AM"]]],
          ],
          max_seq_length=2,
          expected=[
              # Expected first segment has shape [3, (2, 2, 2)]
              [[[b"hello"], [b"there"]], [[b"name", b"is"]],
               [[b"what", b"time"], [], [], []]],
              # Expected second segment has shape [3, (0, 0, 0)]
              [[[], []], [[], [], [], []], [[], []]]
          ],
      ),
      dict(
          descr="Test when segments are rank 3 RaggedTensors and axis = 1",
          segments=[
              # first segment
              [[[b"hello"], [b"there"]], [[b"name", b"is"]],
               [[b"what", b"time"], [b"is"], [b"it"], [b"?"]]],
              # second segment
              [[[b"whodis"], [b"?"]], [[b"bond"], [b","], [b"james"],
                                       [b"bond"]], [[b"5:30"], [b"AM"]]],
          ],
          axis=1,
          max_seq_length=2,
          expected=[
              [[[b"hello"], [b"there"]], [[b"name", b"is"]],
               [[b"what", b"time"], [b"is"]]],
              [[], [[b"bond"]], []],
          ],
      ),
  ])
  def testPerBatchBudgetTrimmer(self,
                                max_seq_length,
                                segments,
                                expected,
                                axis=-1,
                                descr=None):
    max_seq_length = constant_op.constant(max_seq_length)
    trimmer = trimmer_ops.WaterfallTrimmer(max_seq_length, axis=axis)
    segments = [ragged_factory_ops.constant(seg) for seg in segments]
    expected = [ragged_factory_ops.constant(exp) for exp in expected]
    actual = trimmer.trim(segments)
    for expected_seg, actual_seg in zip(expected, actual):
      self.assertAllEqual(expected_seg, actual_seg)


if __name__ == "__main__":
  test.main()
