# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This file contains compression algorithms for the library's data.

The point of entry is CompressData method which is used by the main script
to compress its data. Because of that the compression algorithm may be changed
easily without modifying any of the main script.
"""


def CompressData(data):
  # For the prototyping purposes the compression function is simplified to make
  # debugging easier.
  # TODO(https://crbug.com/998082): write a compression function.
  return data
