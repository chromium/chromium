# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import os
import shutil
import tempfile

import setup_modules

import chromium_src.tools.metrics.python_support.dependency_solver as dependency_solver


class TestDependencyResolver(unittest.TestCase):

  def setUp(self):
    # Create a temporary directory for each test to ensure isolation
    self.test_dir = tempfile.mkdtemp(prefix='dependency_solver_test_')
    self.root_path = self.test_dir

  def tearDown(self):
    # Clean up the temporary directory after each test
    shutil.rmtree(self.test_dir)

  def _create_file(self, relative_path, content=""):
    """Helper to create files with necessary directories."""
    full_path = os.path.join(self.root_path, relative_path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, "w", encoding="utf-8") as f:
      # TEST_CASE is used to not trip the actual presubmit check
      # remove those markers when saving the file
      f.write(content.replace("TEST_CASE ", ""))
    return full_path

  def test_direct_import_module(self):
    """Test: import chromium_src.tools.metrics.foo -> foo.py"""
    # Setup: Main script and the dependency
    self._create_file("main.py",
                      "import TEST_CASE chromium_src.tools.metrics.utils")
    self._create_file("utils.py")

    deps = dependency_solver.scan_directory_dependencies(self.root_path)

    self.assertIn("main.py", deps)
    self.assertIn("utils.py", deps)
    self.assertEqual(deps["main.py"], ["utils.py"])

  def test_import_with_alias(self):
    """Test: import chromium_src.tools.metrics.foo as bar -> foo.py"""
    self._create_file("main.py",
                      "import TEST_CASE chromium_src.tools.metrics.data as d")
    self._create_file("data.py")

    deps = dependency_solver.scan_directory_dependencies(self.root_path)

    self.assertEqual(deps["main.py"], ["data.py"])

  def test_from_import_module(self):
    """Test: from chromium_src.tools.metrics.pkg import mod -> pkg/mod.py"""
    self._create_file(
        "main.py",
        "from chromium_src.tools.metrics.core import TEST_CASE logic")
    self._create_file("core/logic.py")

    deps = dependency_solver.scan_directory_dependencies(self.root_path)

    # Note: formatting usually normalizes paths, we check relative path matching
    expected = os.path.join("core", "logic.py")
    self.assertEqual(deps["main.py"], [expected])

  def test_from_import_multiple_symbols(self):
    """Test: from ... import A, B -> checks A.py and B.py"""
    self._create_file(
        "main.py",
        "from chromium_src.tools.metrics.widgets import" \
        " TEST_CASE button, slider"
    )
    self._create_file("widgets/button.py")
    self._create_file("widgets/slider.py")

    deps = dependency_solver.scan_directory_dependencies(self.root_path)

    expected = [
        os.path.join("widgets", "button.py"),
        os.path.join("widgets", "slider.py")
    ]
    self.assertCountEqual(deps["main.py"], expected)

  def test_symbol_fallback_to_file(self):
    """
        Test: from ...pkg import MyClass
        If pkg/MyClass.py does not exist, it should depend on pkg.py.
        """
    self._create_file(
        "main.py",
        "from chromium_src.tools.metrics.lib import TEST_CASE MyClass")
    # 'lib/MyClass.py' does NOT exist, but 'lib.py' does
    self._create_file("lib.py")

    deps = dependency_solver.scan_directory_dependencies(self.root_path)

    self.assertEqual(deps["main.py"], ["lib.py"])

  def test_import_directory_recursive(self):
    """
        Test: from ... import subdir
        If 'subdir' is a folder, it should include all .py files inside it.
        """
    self._create_file("main.py",
                      "from chromium_src.tools.metrics import TEST_CASE sub")
    self._create_file("sub/one.py")
    self._create_file("sub/nested/two.py")

    deps = dependency_solver.scan_directory_dependencies(self.root_path)

    expected = set([
        os.path.join("sub", "nested", "two.py"),
        os.path.join("sub", "one.py")
    ])
    self.assertEqual(set(deps["main.py"]), expected)

  def test_ignore_unrelated_imports(self):
    """Test that standard imports or other prefixes are ignored."""
    content = """
import TEST_CASE os
import TEST_CASE sys
import TEST_CASE chromium_src.other_tool.stuff
import TEST_CASE chromium_src.tools.metrics.target
        """
    self._create_file("main.py", content)
    self._create_file("target.py")

    deps = dependency_solver.scan_directory_dependencies(self.root_path)

    # Should only find target.py, ignoring os, sys, and other_tool
    self.assertEqual(deps["main.py"], ["target.py"])

  def test_get_all_dependencies_transitive(self):
    """Test resolving the full dependency chain (A->B->C)."""
    # Graph: main -> middle -> leaf
    self._create_file("main.py",
                      "import TEST_CASE chromium_src.tools.metrics.middle")
    self._create_file("middle.py",
                      "import TEST_CASE chromium_src.tools.metrics.leaf")
    self._create_file("leaf.py", "")

    # Build graph
    graph = dependency_solver.scan_directory_dependencies(self.root_path)

    # Resolve for main
    all_deps = dependency_solver.get_all_dependencies(graph, "main.py")

    self.assertCountEqual(all_deps, ["middle.py", "leaf.py"])

  def test_circular_dependency_safety(self):
    """Test that circular dependencies don't cause infinite loops."""
    # Graph: A -> B -> A
    self._create_file("a.py", "import TEST_CASE chromium_src.tools.metrics.b")
    self._create_file("b.py", "import TEST_CASE chromium_src.tools.metrics.a")

    graph = dependency_solver.scan_directory_dependencies(self.root_path)

    # Should finish successfully and contain both
    deps_a = dependency_solver.get_all_dependencies(graph, "a.py")
    self.assertEqual(deps_a, {"b.py", "a.py"})

  def test_missing_file_error(self):
    """Test that importing a non-existent file raises FileNotFoundError."""
    self._create_file("main.py",
                      "import TEST_CASE chromium_src.tools.metrics.ghost")

    # The logic inside _dependencies_of calls _resolve_fs_path
    with self.assertRaises(FileNotFoundError):
      dependency_solver.scan_directory_dependencies(self.root_path)


if __name__ == '__main__':
  unittest.main()
