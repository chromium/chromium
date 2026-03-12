#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
import zlib


class DevilUtilTest(unittest.TestCase):
  executable = None

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.addCleanup(shutil.rmtree, self.temp_dir)

  def run_util(self, *args, stdin=None):
    cmd = [self.executable] + list(args)
    return subprocess.run(cmd,
                          capture_output=True,
                          text=True,
                          check=True,
                          input=stdin)

  def test_hash(self):
    file1 = os.path.join(self.temp_dir, 'file1')
    content1 = b'hello world'
    with open(file1, 'wb') as f:
      f.write(content1)

    file2 = os.path.join(self.temp_dir, 'file2')
    content2 = b'foobar'
    with open(file2, 'wb') as f:
      f.write(content2)

    expected1 = hex(zlib.crc32(content1) & 0xffffffff)[2:]
    expected2 = hex(zlib.crc32(content2) & 0xffffffff)[2:]

    result = self.run_util('hash', f'{file1}:{file2}')
    self.assertEqual(result.stdout.strip(), f'{expected1}\n{expected2}')

  def test_hash_missing(self):
    missing_file = os.path.join(self.temp_dir, 'missing')
    result = self.run_util('hash', missing_file)
    self.assertEqual(result.stdout, '\n')

  def test_compress(self):
    dest = os.path.join(self.temp_dir, 'compressed.zst')
    content = 'test content\nline2'
    self.run_util('compress', dest, content)
    self.assertTrue(os.path.exists(dest))

    # Verify via response file expansion
    file1 = os.path.join(self.temp_dir, 'f1')
    with open(file1, 'w') as f:
      f.write('c1')

    # Compress the path of file1 so that @dest.zst expands to file1's path
    self.run_util('compress', dest, file1 + '\n')

    expected = hex(zlib.crc32(b'c1') & 0xffffffff)[2:]
    result = self.run_util('hash', f'@{dest}')
    self.assertEqual(result.stdout.strip(), expected)

  def test_archive_and_extract(self):
    src_dir = os.path.join(self.temp_dir, 'src')
    os.makedirs(src_dir)
    f1 = os.path.join(src_dir, 'file1')
    with open(f1, 'w') as f:
      f.write('content1')
    f2 = os.path.join(src_dir, 'file2')
    with open(f2, 'w') as f:
      f.write('content2')

    members_file = os.path.join(self.temp_dir, 'members')
    # Format is: host_path\narchive_path\n...
    with open(members_file, 'w') as f:
      f.write(f'{f1}\nsub/file1_in_arc\n')
      f.write(f'{f2}\nfile2_in_arc\n')

    archive = os.path.join(self.temp_dir, 'arc.zst')
    self.run_util('archive', archive, members_file)

    extract_dir = os.path.join(self.temp_dir, 'extract')
    os.makedirs(extract_dir)

    cwd = os.getcwd()
    os.chdir(extract_dir)
    try:
      self.run_util('extract', archive)
    finally:
      os.chdir(cwd)

    with open(os.path.join(extract_dir, 'sub/file1_in_arc'), 'r') as f:
      self.assertEqual(f.read(), 'content1')
    with open(os.path.join(extract_dir, 'file2_in_arc'), 'r') as f:
      self.assertEqual(f.read(), 'content2')

  def test_archive_and_extract_stdin_stdout(self):
    src_dir = os.path.join(self.temp_dir, 'src')
    os.makedirs(src_dir)
    f1 = os.path.join(src_dir, 'file1')
    with open(f1, 'w') as f:
      f.write('content1')

    members_file = os.path.join(self.temp_dir, 'members')
    with open(members_file, 'w') as f:
      f.write(f'{f1}\nfile1_in_arc\n')

    # Run archive to stdout
    cmd = [self.executable, 'archive', '-', members_file]
    result = subprocess.run(cmd, capture_output=True, check=True)
    archive_content = result.stdout

    # Run extract from stdin
    extract_dir = os.path.join(self.temp_dir, 'extract_stdin')
    os.makedirs(extract_dir)
    cwd = os.getcwd()
    os.chdir(extract_dir)
    try:
      subprocess.run([self.executable, 'extract', '-'],
                     input=archive_content,
                     check=True)
    finally:
      os.chdir(cwd)

    with open(os.path.join(extract_dir, 'file1_in_arc'), 'r') as f:
      self.assertEqual(f.read(), 'content1')

  def test_pipe(self):
    pipe_path = os.path.join(self.temp_dir, 'test_pipe')
    self.run_util('pipe', pipe_path)
    self.assertTrue(os.path.exists(pipe_path))
    self.assertTrue(stat.S_ISFIFO(os.stat(pipe_path).st_mode))

  def test_response_file(self):
    file1 = os.path.join(self.temp_dir, 'f1')
    with open(file1, 'w') as f:
      f.write('c1')

    resp_file = os.path.join(self.temp_dir, 'resp')
    with open(resp_file, 'w') as f:
      f.write('hash\n')
      f.write(file1 + '\n')

    expected = hex(zlib.crc32(b'c1') & 0xffffffff)[2:]
    # Run without 'hash' command explicitly, it should come from resp file
    cmd = [self.executable, f'@{resp_file}']
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    self.assertEqual(result.stdout.strip(), expected)

  def test_hash_with_parens(self):
    paren_dir = os.path.join(self.temp_dir, 'dir_(dbg)')
    os.makedirs(paren_dir)
    test_file = os.path.join(paren_dir, 'test.apk')
    content = b'content'
    with open(test_file, 'wb') as f:
      f.write(content)

    expected = hex(zlib.crc32(content) & 0xffffffff)[2:]
    result = self.run_util('hash', test_file)
    self.assertEqual(result.stdout.strip(), expected)


if __name__ == '__main__':
  parser = argparse.ArgumentParser('Tests for devil_util_bin')
  parser.add_argument('executable', help='Path to devil_util_bin')
  args, unknown = parser.parse_known_args()
  DevilUtilTest.executable = os.path.abspath(args.executable)

  unittest.main(argv=[sys.argv[0]] + unknown)
