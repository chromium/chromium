# python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from lib import common


class TestCommon(unittest.TestCase):
    def test_crate_name_normalized(self):
        r = common.crate_name_normalized("foo")
        self.assertEqual(r, "foo")
        r = common.crate_name_normalized("foo-bar")
        self.assertEqual(r, "foo_bar")
        r = common.crate_name_normalized("foo-bar-baz")
        self.assertEqual(r, "foo_bar_baz")
        r = common.crate_name_normalized("foo.bar-baz.blep")
        self.assertEqual(r, "foo_bar_baz_blep")
        r = common.crate_name_normalized("foo_bar")
        self.assertEqual(r, "foo_bar")

    def test_version_epoch_dots(self):
        r = common.version_epoch_dots("1.2.3")
        self.assertEqual(r, "1")
        r = common.version_epoch_dots("1.2")
        self.assertEqual(r, "1")
        r = common.version_epoch_dots("1")
        self.assertEqual(r, "1")
        r = common.version_epoch_dots("0.1")
        self.assertEqual(r, "0.1")
        r = common.version_epoch_dots("0.0.1")
        self.assertEqual(r, "0.0.1")

    def test_version_epoch_normalized(self):
        r = common.version_epoch_normalized("1.2.3")
        self.assertEqual(r, "1")
        r = common.version_epoch_normalized("1.2")
        self.assertEqual(r, "1")
        r = common.version_epoch_normalized("1")
        self.assertEqual(r, "1")
        r = common.version_epoch_normalized("0.1")
        self.assertEqual(r, "0_1")
        r = common.version_epoch_normalized("0.0.1")
        self.assertEqual(r, "0_0_1")

    def test_find_chromium_root(self):
        cwd = os.getcwd()
        # If run from elsewhere then the test will fail. If the code is broken
        # then it would fail too =)
        from_root = common._find_chromium_root(cwd)
        self.assertEqual(["tools", "crates"],
                         from_root,
                         msg="Run tests from the '//tools/crates/' directory.")

        root = os.path.split(os.path.split(cwd)[0])[0]
        from_root = common._find_chromium_root(root)
        self.assertEqual([], from_root)

        subdir = os.path.join(root, "foo", "bar", "baz")
        from_root = common._find_chromium_root(subdir)
        self.assertEqual(["foo", "bar", "baz"], from_root)

    def test_gn_third_party_path(self):
        self.assertEqual(["tools", "crates"],
                         common._PATH_FROM_CHROMIUM_ROOT,
                         msg="Run tests from the '//tools/crates/' directory.")

        for i in range(2):
            if i == 0:
                root = "//tools/crates/third_party/rust"
            else:
                # Pretend we're running the tool from the root of src.git.
                old = common._PATH_FROM_CHROMIUM_ROOT
                common._PATH_FROM_CHROMIUM_ROOT = []
                root = "//third_party/rust"

            r = common.gn_third_party_path()
            self.assertEqual(r, root, msg="i == {}".format(i))
            # Test relpath.
            r = common.gn_third_party_path(rel_path=["a", "b"])
            self.assertEqual(r, root + "/a/b", msg="i == {}".format(i))

            if i != 0:
                common._PATH_FROM_CHROMIUM_ROOT = old

    def test_gn_crate_path(self):
        self.assertEqual(["tools", "crates"],
                         common._PATH_FROM_CHROMIUM_ROOT,
                         msg="Run tests from the '//tools/crates/' directory.")
        root = "//tools/crates/third_party/rust"

        # Test crate normalization.
        r = common.gn_crate_path("foo-bar", "1.2.3")
        self.assertEqual(r, root + "/foo_bar/v1")
        # Test partial version.
        r = common.gn_crate_path("foo-bar", "2.3")
        self.assertEqual(r, root + "/foo_bar/v2")
        # Test 0.x version.
        r = common.gn_crate_path("foo-bar", "0.3")
        self.assertEqual(r, root + "/foo_bar/v0_3")
        # Test 0.0.x version.
        r = common.gn_crate_path("foo-bar", "0.0.4")
        self.assertEqual(r, root + "/foo_bar/v0_0_4")
        # Test relpath.
        r = common.gn_crate_path("foo-bar", "5", rel_path=["a", "b"])
        self.assertEqual(r, root + "/foo_bar/v5/a/b")

    def test_os_crate_name_dir(self):
        # Test normalization of crate names.
        r = common.os_crate_name_dir("foo-bar")
        self.assertEqual(r, os.path.join("third_party", "rust", "foo_bar"))
        # Test rel_path.
        r = common.os_crate_name_dir("foo-bar", rel_path=["a", "b"])
        self.assertEqual(
            r, os.path.join("third_party", "rust", "foo_bar", "a", "b"))

    def test_os_crate_version_dir(self):
        # Test partial version.
        r = common.os_crate_version_dir("foo", "2")
        self.assertEqual(r, os.path.join("third_party", "rust", "foo", "v2"))
        # Test 0.x version.
        r = common.os_crate_version_dir("foo", "0.3.1")
        self.assertEqual(r, os.path.join("third_party", "rust", "foo", "v0_3"))
        # Test 0.0.x version.
        r = common.os_crate_version_dir("foo", "0.0.4")
        self.assertEqual(r, os.path.join("third_party", "rust", "foo",
                                         "v0_0_4"))
        # Test full version.
        r = common.os_crate_version_dir("foo", "5.3.1")
        self.assertEqual(r, os.path.join("third_party", "rust", "foo", "v5"))
        # Test rel_path.
        r = common.os_crate_version_dir("foo", "6", rel_path=["c.d", "e.f"])
        self.assertEqual(
            r, os.path.join("third_party", "rust", "foo", "v6", "c.d", "e.f"))

    def test_os_crate_cargo_dir(self):
        # Test the inner dir is there.
        r = common.os_crate_cargo_dir("foo-bar", "1.2.3")
        self.assertEqual(
            r, os.path.join("third_party", "rust", "foo_bar", "v1", "crate"))
        # Test rel_path.
        r = common.os_crate_cargo_dir("foo-bar", "1.2.3", rel_path=["g", "h"])
        self.assertEqual(
            r,
            os.path.join("third_party", "rust", "foo_bar", "v1", "crate", "g",
                         "h"))

    def test_crate_download_url(self):
        r = common.crate_download_url("foo", "1.2.3")
        self.assertEqual(r,
                         "https://static.crates.io/crates/foo/foo-1.2.3.crate")

    def test_crate_view_url(self):
        r = common.crate_view_url("foo")
        self.assertEqual(r, "https://crates.io/crates/foo")
