#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test wrapper/executor for model_unittest.py."""

import sync.model_unittest
import unittest

# model_unittest.py lives in the sync directory (as the unit test is synced
# across different repositories). However, cannot run model_unittest.py
# directly as a unit test, since model.py in this repository (Chromium)
# is meant to be run from the tools/metrics/structured directory. Running
# model_unittest.py directly in this repository will cause
# ModuleNotFoundErrors as it does not know about the sync module.


class ModelTest(sync.model_unittest.ModelTest):
  pass


if __name__ == '__main__':
  unittest.main()
