#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import argparse
import csv
import logging
import os
import sys
"""This module contains the utilities necessary to read Dtrace result files and
convert them in the collapse stack format used by FlameGraph tools.
"""


class StackCollapser:
  """Massages and collapses chromium Dtrace profiles.

  Collapsing means taking samples in the DTrace format from multiple files and
  converting them to the "collapsed stack" format that is used in tools like
  flamegraphs. The format is called "collapsed" because it puts the whole stack
  on a single line.

  The massaging part consists in cutting out and adding stack frames to make
  the profile easier to use for the purpose of analyzing Chromium performance.

  Typical usage example:

  collapser = StackCollapser('./samples/samples.collapsed')
  collapser.read_dtrace_logs('./profile/')
  collapser.post_process_samples()
  collapse.write_results()
  """

  def __init__(self, output_filename):
    """
    Args:
      output_filename: The path of the file in which results are written.
    """
    self.output_filename = output_filename
    self.samples = []
    self.post_processing_applied = False

  def set_samples_for_testing(self, samples):
    """
    Args:
      samples: Values extracted from profiles. Type is:
      {"frames": list of str, "weight": int}
    """
    self.samples = samples

  def read_dtrace_logs(self, stack_dir):
    """
    Args:
      stack_dir: The directory where Dtrace profile results can be found.

    Returns:
      A list of string arrays that contain stack frames and a count.

    Raises:
      SystemExit: When no results are found in stack_dir.
    """
    # The DTrace format is defined as such:
    # First there are lines, each containing
    # the name of a function with an optional offset.
    # Finally there is a line with the weight associated
    # with the full stack. The block is broken up by an
    # empty line and a new stack starts.
    #
    # base::foo+0x21
    # content::bar
    # biz::baz
    #  17
    #
    # ...

    weights = defaultdict(int)
    for root, dirs, files in os.walk(stack_dir):
      for stack_file in files:
        with open(os.path.join(stack_dir, stack_file),
                  newline='',
                  encoding="ISO-8859-1") as stack_file:
          lines = stack_file.readlines()

          # Read each such blocks in all DTrace results
          # and store in the return format.
          block = []
          for line in lines:
            if not line.strip():
              # If an empty line is encountered.
              if block:
                # If that empty line was terminating a block.

                # Keep the count.
                weight = block.pop()

                # Reorder the frames since they were reversed while reading.
                block.reverse()

                # Build the full stack line.
                stack_trace_string = ";".join(block)

                # Increment the sum of weights for this specific full stack.
                weights[stack_trace_string] += int(weight)

                # Start a new block.
                block = []
            else:
              # Read the line to build on the current block.
              stack_frame = line.strip()

              # Remove offset
              plus_index = stack_frame.find('+')
              if plus_index != -1:
                stack_frame = stack_frame[:plus_index]

              block.append(stack_frame)

    for stack, weight in weights.items():
      sample = {}
      sample["frames"] = stack.split(';')
      sample["weight"] = weight
      self.samples.append(sample)

    if not self.samples:
      logging.error("No results found, check directory contents")
      sys.exit(-1)

  def shorten_stack(self, stack):
    """Drop some frames that don't offer any valuable information. The part
    above/before the frame is trimmed. This means that the base of the stack
    can be dropped but no frame can "skipped".

    Example (dropping biz):

    foo;bar;biz;boo --> boo
    foo;biz;bar;boo --> bar;boo

    Args:
      stack: An array of strings that represent each frame of a stack trace.

    Returns: The input array with zero or more elements removed.
    """

    message_pump_roots = [
        "base::MessagePumpNSRunLoop::DoRun", "base::MessagePumpDefault::Run",
        "base::MessagePumpKqueue::Run", "base::MessagePumpCFRunLoopBase::Run",
        "base::MessagePumpNSApplication::DoRun", "base::mac::CallWithEHFrame"
    ]

    first_ignored_index = -1
    for i, frame in enumerate(stack):
      if any(
          frame.startswith(message_pump_root)
          for message_pump_root in message_pump_roots):
        # If any of the markers is present in the function it means everything
        # under the frame should be dropped from the stack.
        first_ignored_index = max(
            i - 1,
            0)  # Cutoff point is included but can't be smaller than zero.
        break

    if first_ignored_index != -1:
      return stack[first_ignored_index + 1:]
    else:
      return stack

  def add_category_from_any_frame(self, stack):
    """Adds synthetic frame according to some generic categories to help
    analyze the results.

    Args:
      stack: An array of strings that represent each frame of a stack trace.

    Returns: The input array with zero or one element added.
    """

    # Categories ordered by importance. Each element of the list is an
    # array of synonyms.
    special_markers = [['viz'], ['net::', 'network::'], ['blink::'], ['mojo::'],
                       ['gpu::'], ['v8::'], ['sql::'], ['CoreText'], ['AppKit'],
                       ['Security'], ['CoreFoundation']]

    # Look for the presence of any of the special markers in the stack and
    # compound them to create the synthetic frame.
    compound_marker = []
    for synonyms in special_markers:
      for variation in synonyms:
        for frame in stack:
          if variation in frame and variation not in compound_marker:
            compound_marker.append(synonyms[0])

    # Add some namespace separators for markers that didn't have them.
    for i, marker in enumerate(compound_marker):
      if marker.find("::") == -1:
        compound_marker[i] = marker + "::"

    if compound_marker:
      compound_marker.sort()
      stack = ["".join(compound_marker)] + stack

    return stack

  def remove_tokens(self, stack):
    """Removes some substrings from frames in the stack.

    Args:
      stack: An array of strings that represent each frame of a stack trace.

    Returns: The input array with zero or more frames modified.
    """

    # Drop parts of the function names that just add noise.
    tokens_to_remove = [
        "Chromium Framework`", "libsystem_kernel.dylib`", "Security`"
    ]

    for i, frame in enumerate(stack):
      for token in tokens_to_remove:
        # If removing the token would result in an empty string don't
        # remove it.
        if stack[i] != token:
          stack[i] = stack[i].replace(token, "")

    return stack

  def write_down_samples(self):
    """Writes down self.samples to a file. In contrast to the Dtrace format full
    stacks are writtent on a single line. At first the different are separated
    by semi-colons and a space separates the weight associated with the
    function.

    Example:

    base::foo;content::bar;biz::baz 17
    base::biz;content::boo;biz::bim 23
    ...

    """

    if not os.path.exists(os.path.dirname(self.output_filename)):
      os.makedirs(os.path.dirname(self.output_filename))

    with open(self.output_filename, 'w') as f:
      for row in self.samples:
        line = ';'.join(row["frames"])
        weight = row["weight"]
        # Reform the line in stacked format and write it out.
        f.write(f"{line} {weight}\n")

  def post_process_samples(self):
    """Applies filtering and enhancing to self.samples().  This function can
    only be called once.

    Raises:
      SystemExit: If this function is called twice on the same object.
    """

    if self.post_processing_applied:
      logging.error("Post processing cannot be applied twice")
      sys.exit(-1)
    self.post_processing_applied = True

    processed_samples = []
    for row in self.samples:
      # Filter out the frames we don't care about and all those under it.
      row["frames"] = self.shorten_stack(row["frames"])
      row["frames"] = self.add_category_from_any_frame(row["frames"])
      row["frames"] = self.remove_tokens(row["frames"])


def main(stack_dir, output_filename):
  collapser = StackCollapser(output_filename)
  collapser.read_dtrace_logs(stack_dir)
  collapser.post_process_samples()
  collapser.write_down_samples()


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description='Flip stack order of a collapsed stack file.')
  parser.add_argument("--stack_dir",
                      help="Collapsed stack file.",
                      required=True)
  parser.add_argument("--output_filename",
                      help="The file to write the collapsed stacks into.",
                      required=True)
  args = parser.parse_args()
  main(args.stack_dir, args.output_filename)
