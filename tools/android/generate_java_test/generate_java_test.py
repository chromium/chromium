#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
This script helps to generate Java instrumentation tests and/or Unit test
(robolectric) for new feature Other than that, it helps to add your source
file (foo.java) and test files (footest.java) to gn (java_sources.gni)
Also, it will generate OWNER file with your ldap@chromium.org if it doesn't exit

Where to run?
Anywhere in the repo as the tool can find the abstract paths.

How to run? sample:
If you are building the following example like
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"

Use command:
python generate_java_test.py --instrumentation --unittest --source \
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"
   -i, --instrumentation   generate instrumentation test file
   -u, --unittest          generate unittest file
   Default will generate both of them
   -s, --source            source file with path [must have]

What do you get?
- test files created
- gn file updated
- OWNER created

What do you need to do?
- Write TESTs!


'''
from __future__ import print_function

import argparse
import datetime
import os
import re
import bisect

# Below sessions are contents for test files
this_year = str(datetime.datetime.now().year)

_INST_TEST_FILE = '''// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// generate_java_test.py

package %s;

import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;

/** Instrumentation tests for {@link %s}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class %sInstrumentationTest extends DummyUiActivityTestCase {

}
'''

_UNIT_TEST_FILE = '''// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// generate_java_test.py

package %s;

import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link %s}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class %sTest {

}

'''

# End or contents

# Set root to be "src/" like src/tools/android/gn_java_test/../../../
root_path = os.path.abspath(os.path.dirname(__file__)) + "/../../../"

# path of gni files
javatest_gni = os.path.join(root_path,
                            "chrome/android/chrome_test_java_sources.gni")
junittest_gni = os.path.join(
    root_path, "chrome/android/chrome_junit_test_java_sources.gni")

# Help message if the user use wrong arguments
HELP_MESSAGE = '''
Wrong --source pattern! Please use the sample below
generate_create_test.py --unittest --source \
chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java
'''


# used for better logging
class bcolors:
  HEADER = '\033[95m'
  OKBLUE = '\033[94m'
  OKGREEN = '\033[92m'
  WARNING = '\033[93m'
  FAIL = '\033[91m'
  ENDC = '\033[0m'
  BOLD = '\033[1m'
  UNDERLINE = '\033[4m'


def failmsg(s):
  return "%s%s%s" % (bcolors.FAIL, s, bcolors.ENDC)


def infomsg(s):
  return "%s%s%s" % (bcolors.OKBLUE, s, bcolors.ENDC)


def warningmsg(s):
  return "%s%s%s" % (bcolors.WARNING, s, bcolors.ENDC)


# -----------------  APIs and main function are below ----------------------#


# Verify if source file exist, return () if it doesn't exist,
# otherwise, return (package_name, file_name)
def GetPackageAndFile(source):
  if not os.path.exists(os.path.join(root_path, source)):
    print(failmsg("%s%s Does not exist!" % (root_path, source)))
    return ()
#TODO(yzjr): Will add support for components under chrome/android/features
#crbug/950783
  matchSource = re.match(
      "chrome\/android\/java\/src\/" +
      "(org\/chromium\/chrome\/browser\/.*\/?)\/(.*)\.java", source)

  if not matchSource:
    print(failmsg(HELP_MESSAGE))
    return ()

  package_name = matchSource.group(1)
  print(infomsg("Package name: %s" % package_name))
  file_name = matchSource.group(2)
  print(infomsg("File name: %s" % file_name))
  return (package_name, file_name)

# Generate an instrumentation test file, OWNER and modify corresponding GNI
def CreateInstrumentationTestFile(package_path, file_name):
  package_name = package_path.replace('/', '.')

  ins_testfile_path = os.path.join(root_path, 'chrome/android/javatests/src/',
                                   package_path)
  ins_testfile = os.path.join(ins_testfile_path, (file_name +
    'InstrumentationTest.java'))
  print(infomsg("+++ Creating instrumentation_test file: %s" % ins_testfile))

  if os.path.exists(ins_testfile):
    print(warningmsg("%s already exist!\n" % ins_testfile))
  else:
    dir_name = os.path.dirname(ins_testfile)
    if not os.path.exists(dir_name):
      os.makedirs(dir_name)
    try:
      file = open(ins_testfile, "w")
      filecontent = _INST_TEST_FILE % (this_year, package_name, file_name,
                                       file_name)
      file.write(filecontent)
      file.close()
    except Exception as e:
      print(warningmsg(e.message))
  # Create Owner file if source code OWNERS exists, otherwise skip
  source_ownerfile = os.path.join('chrome/android/java/src/', package_path,
                                  'OWNERS')
  if os.path.exists(os.path.join(root_path, source_ownerfile)):
    ins_owner = os.path.join(ins_testfile_path, 'OWNERS')
    CreateOwnerFile(ins_owner, source_ownerfile)
  # Modify GN file
  tag = "chrome_test_java_sources"
  txt = 'javatests/src/%s/%sInstrumentationTest.java' % (package_path,
      file_name)
  ModifyGnFile(javatest_gni, tag, txt)


# Generate a unit test file, OWNER and modify corresponding GNI
def CreateUnitTestFile(package_path, file_name):
  package_name = package_path.replace('/', '.')

  unit_testfile_path = os.path.join(root_path, 'chrome/android/junit/src/',
                                    package_path)
  unit_testfile = os.path.join(unit_testfile_path, file_name + "Test.java")
  print(infomsg("+++ Creating unit test file: %s" % unit_testfile))

  if os.path.exists(unit_testfile):
    print(warningmsg("%s already exist!\n" % unit_testfile))
  else:
    dir_name = os.path.dirname(unit_testfile)
    if not os.path.exists(dir_name):
      os.makedirs(dir_name)
    try:
      file = open(unit_testfile, "w")
      filecontent = _UNIT_TEST_FILE % (this_year, package_name, file_name,
                                       file_name)
      file.write(filecontent)
      file.close()
    except Exception as e:
      print(warningmsg(e.message))
  # Create Owner file if source code OWNERS exists, otherwise skip
  source_ownerfile = os.path.join('chrome/android/java/src/', package_path,
                                  'OWNERS')
  if os.path.exists(os.path.join(root_path, source_ownerfile)):
    unit_testowner = os.path.join(unit_testfile_path, "OWNERS")
    CreateOwnerFile(unit_testowner, source_ownerfile)
  # Modify GN file
  tag = "chrome_junit_test_java_sources"
  txt = 'junit/src/%s/%sTest.java' % (package_path, file_name)
  ModifyGnFile(junittest_gni, tag, txt)

# Create OWNER file if it doesn't exist
def CreateOwnerFile(ownerfile, source_owners):
  if os.path.exists(ownerfile):
    print(warningmsg("%s already exists!" % ownerfile))
    return
  try:
    file = open(ownerfile, "w")
    file.write("file://" + source_owners)
    file.close()
  except Exception as e:
    print(bcolors.WARNING + e.message + bcolors.ENDC)

# Modify GN file
def ModifyGnFile(gn_file, tag, txt):
  read_lines = []
  filelist = {}

  try:
    with open(gn_file) as f:
      code = compile(f.read(), "sometempfile.py", 'exec')
      exec (code, filelist)
      if tag not in filelist:
        print(warningmsg("%s is not found\n" % tag))
        return
      if txt in filelist[tag]:
        print(warningmsg("%s already exists\n" % txt))
        return

      # sorted the list as it might not be sorted from user input
      testfiles = sorted(filelist[tag])
      # insert test file to an sorted list
      bisect.insort(testfiles, txt)
    f.close()
    # read file headers with comments or others until tag
    with open(gn_file) as f:
      for line in f:
        stripped = line.strip()
        if stripped.startswith(tag):
          read_lines.append(line)
      for files in testfiles:
        read_lines.append("  \"%s\",\n" % files)
        # end of tag
      read_lines.append(']')
    f.close()
    with open(gn_file, 'w') as f:
      f.write(''.join(read_lines))
    f.close()
  except Exception as e:
    print(warningmsg("Failed to modify %s because %s" % (gn_file, e.message)))


# *******************  Below is main function ***********************
def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "-u", "--unittest", help="create unit tests", action="store_true")
  parser.add_argument(
      "-i",
      "--instrumentation",
      help="create java instrumentation tests",
      action="store_true")
  parser.add_argument(
      "-s",
      "--source",
      type=str,
      help="source of java code" +
      "like chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java",
      required=True)

  args = parser.parse_args()

  source_path = GetPackageAndFile(args.source or '')
  if not source_path:
    exit()

  package_path = source_path[0]
  file_name = source_path[1]

  # Add test files to java_sources.gni if they don't exist
  ran = False
  if args.unittest:
    ran = True
    CreateUnitTestFile(package_path, file_name)
  if args.instrumentation:
    ran = True
    CreateInstrumentationTestFile(package_path, file_name)
  if not ran:  # default for both tests
    CreateInstrumentationTestFile(package_path, file_name)
    CreateUnitTestFile(package_path, file_name)


if __name__ == "__main__":
  main()
'''

Test cases for this script:

* wrong commands *
python generate_java_test.py
python generate_java_test.py -h
python generate_java_test.py --instrumentation
python generate_java_test.py --unittest
python generate_java_test.py --instrumentation --unittest --source\
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"

* good commands *
python generate_java_test.py --instrumentation --unittest --source\
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"
python generate_java_test.py --source\
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"
python generate_java_test.py --unittest --source\
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"
python generate_java_test.py --instrumentation --source\
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"

* File exists - run twice *
python generate_java_test.py --source\
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"
python generate_java_test.py --source\
"chrome/android/java/src/org/chromium/chrome/browser/foo/Foo.java"
'''
