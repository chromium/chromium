#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# -----------------------------FILE OVERVIEW----------------------------- #
# This script generates a list of tests that exist for given parameters.
#
# This script was created so that we could determine which tests are used
# by specific components, so that coverage can be generated for specific
# components.
#
# Use the built in help for instructions.
# ----------------------------------------------------------------------- #

import os
import sys
import argparse
import fnmatch
import re
import time
import json
from threading import Thread, Lock

sys.path.append(os.path.abspath("./third_party/blink/tools"))
sys.path.append(os.path.abspath("./"))
# Chromium check
NOT_CHROME_ERROR = (
    'You must be in the chromium src directory to run this command.')
try:
  file = open('./DIR_METADATA', 'r')
  if not 'project: "chromium"' in file.read():
    print(NOT_CHROME_ERROR)
    exit()
except IOError:
  print(NOT_CHROME_ERROR)
  exit()

from third_party.blink.tools.blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from third_party.blink.tools.blinkpy.common.host import Host

# CONSTANTS
DIR_METADATA = 'DIR_METADATA'
COMMON_METADATA = 'COMMON_METADATA'
OWNERS = 'OWNERS'
TEST_NAME_REGEX = re.compile(
    r"TEST(_[FP])?\(\s*'?([a-zA-Z][a-zA-Z0-9]*)'?,"
    "\s*'?([a-zA-Z][a-zA-Z0-9_]*)'?", re.MULTILINE)
TEST_SUITE_REGEX = re.compile(
    r'TEST_SUITE(_[FP])?\(\s*([a-zA-Z][a-zA-Z0-9]*),\s*([a-zA-Z][a-zA-Z0-9]*),',
    re.MULTILINE)

parser = argparse.ArgumentParser(
    "Determines test suites for given directories and components")
parser.add_argument(
    '-c',
    '--components',
    nargs='+',
    # TODO: Adjust to buganizer once the
    # coverage team updates the tool
    help='A monorail component to collect suites for',
    default=[])
parser.add_argument(
    '-d',
    '--dirs',
    nargs='*',
    help='Directory to search through, at least one must be present',
    default=['./'])
parser.add_argument('-i',
                    '--ignore_dir',
                    nargs='*',
                    help='Directories to ignore',
                    default=[])
parser.add_argument('-o',
                    '--out_file',
                    help='The file to write the suites to.',
                    required=True)
parser.add_argument('-j',
                    '--jobs',
                    required=False,
                    help='The number of threads to allow',
                    type=int,
                    default=0)
args = parser.parse_args()


class ThreadManager(int):

  def __init__(self, max_threads):
    self.num_threads = 0
    self.max_threads = max_threads
    self.lock = Lock()

  def _thread_wrapper(self, func, args):
    func(*args)
    with self.lock:
      self.num_threads -= 1

  def run(self, func, args):
    if self.num_threads < self.max_threads:
      thread = Thread(target=self._thread_wrapper, args=(func, args))
      with self.lock:
        self.num_threads += 1
      thread.start()
      return True
    else:
      return False

  def wait_until_threads_done(self):
    while self.num_threads > 0:
      time.sleep(0.1)


class TestFinder(argparse.Namespace):

  def __init__(self, args):
    self.host = Host()
    self.component_map = dict()
    self.dirs_with_tests = set()
    self.disabled_tests = dict()
    self.maybe_tests = dict()
    self.total_disabled = 0
    self.total_maybe = 0
    self.total_working = 0
    self.dirs = args.dirs
    self.comps = args.components
    self.dir_thread_manager = ThreadManager(args.jobs * .75)
    self.read_thread_manager = ThreadManager(args.jobs / 4)
    self.out_file = args.out_file

  def is_hidden(self, path):
    name = os.path.basename(os.path.abspath(path))
    return name.startswith('.')

  def is_output_dir(self, path):
    name = os.path.basename(os.path.abspath(path))
    return name.startswith('out')

  def is_test_file(self, filepath):
    if filepath.endswith('.py'):
      return False
    if filepath.endswith('.png'):
      return False
    noext = os.path.splitext(os.path.abspath(filepath))[0]
    name = os.path.basename(noext)
    return name.endswith('test')

  def get_items_in_dir(self, path):
    items_list = os.listdir(path)
    files = list()
    dirs = list()
    for item in items_list:
      item_path = os.path.abspath(os.path.join(path, item))
      if os.path.isfile(item_path):
        files.append(item_path)
      else:
        dirs.append(item_path)
    return [files, dirs]

  def get_component_for_dir(self, path, type):
    # Python does something weird with hidden dirs.
    # We don't care about those anyway.
    if self.is_hidden(path):
      return None
    metadata_path = os.path.join(path, type)
    extractor = DirectoryOwnersExtractor(Host())
    try:
      return extractor.extract_component(metadata_path)
    except KeyError:
      return None

  def get_test_suites_for_file(self, filepath, suites):
    try:
      new_suites = set()
      text = open(filepath, 'r').read()
      relPath = os.path.relpath(filepath)
      # Find regular tests
      matches = re.findall(TEST_NAME_REGEX, text)
      for m in matches:
        new_suites.add(m[1])
        # Check for disabled and maybe tests
        if m[2] != None and m[2].startswith('DISABLED'):
          disabled = self.disabled_tests.get(relPath)
          self.total_disabled += 1
          if disabled == None:
            disabled = []
            self.disabled_tests[relPath] = disabled
          disabled.append(m[1] + "." + m[2])
        elif m[2] != None and m[2].startswith('MAYBE'):
          self.total_maybe += 1
          maybe = self.maybe_tests.get(relPath)
          if maybe == None:
            maybe = []
            self.maybe_tests[relPath] = maybe
          maybe.append(m[1] + "." + m[2])
        else:
          self.total_working += 1
      # Find suites
      matches = re.findall(TEST_SUITE_REGEX, text)
      for m in matches:
        new_suites.add(m[2])
      suites.extend(new_suites)
    except IOError:
      print('Failed to open ' + filepath)
      return list()

  def get_test_suites_for_dir(self, dir, suites):
    files, _ = self.get_items_in_dir(dir)
    files = list(filter(lambda file: self.is_test_file(file), files))
    if files.__len__() > 0:
      self.dirs_with_tests.add(os.path.relpath(dir))
    for file in files:
      if not self.read_thread_manager.run(self.get_test_suites_for_file,
                                          (file, suites)):
        self.get_test_suites_for_file(file, suites)

  def start(self):
    for dir in self.dirs:
      self.build_component_map(dir, None)
    self.dir_thread_manager.wait_until_threads_done()
    self.save()

  def save(self):
    with open(self.out_file, 'w') as outfile:
      data = {
          "components": self.component_map,
          "directories": [*self.dirs_with_tests],
          "disabled_tests": self.disabled_tests,
          "maybe_tests": self.maybe_tests,
          "total_disabled": self.total_disabled,
          "total_maybe": self.total_maybe,
          "total_working": self.total_working,
          "total_tests":
          self.total_working + self.total_maybe + self.total_disabled
      }
      json.dump(data, outfile, indent=2)

  def build_component_map(self, cur_dir, common_component):
    # Normalize cur_dir
    cur_dir = os.path.abspath(cur_dir)
    # Get the component
    component = self.get_component_for_dir(cur_dir, DIR_METADATA)
    using_common_metadata = False
    # If the component is in common metadata, it should pass through to sub dir.
    if component == None and common_component == None:
      component = self.get_component_for_dir(cur_dir, COMMON_METADATA)
      using_common_metadata = True
    if component != None:
      allowed = self.comps.__len__() == 0
      for comp_glob in self.comps:
        result = fnmatch.filter([component], comp_glob)
        allowed = result.__len__() > 0
        if allowed:
          break
      if allowed:
        tests_for_comp = self.component_map.get(component)
        # If the component isn't in the map yet, add it
        if tests_for_comp == None:
          tests_for_comp = list()
          self.component_map[component] = tests_for_comp
        self.get_test_suites_for_dir(cur_dir, tests_for_comp)
    # Even if the dir didn't have a component,
    #   we still need to check it's subdirs.
    _, sub_dirs = self.get_items_in_dir(cur_dir)
    for sub_dir in sub_dirs:
      # skip hidden directories
      if not self.is_hidden(sub_dir) and not self.is_output_dir(sub_dir):
        common_component_to_pass = (component if using_common_metadata else
                                    common_component)
        if not self.dir_thread_manager.run(self.build_component_map,
                                           (sub_dir, common_component_to_pass)):
          self.build_component_map(sub_dir, common_component_to_pass)


try:
  finder = TestFinder(args)
  finder.start()
except KeyboardInterrupt:
  print('\nUser Interrupted')
  exit()
