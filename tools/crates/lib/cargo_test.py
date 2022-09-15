# python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from lib import cargo


class TestCargo(unittest.TestCase):
    def test_output_to_usage(self):
        self.assertEqual(cargo.CrateBuildOutput.NORMAL.as_dep_usage(),
                         cargo.CrateUsage.FOR_NORMAL)
        self.assertEqual(cargo.CrateBuildOutput.BUILDRS.as_dep_usage(),
                         cargo.CrateUsage.FOR_BUILDRS)
        self.assertEqual(cargo.CrateBuildOutput.TESTS.as_dep_usage(),
                         cargo.CrateUsage.FOR_TESTS)
