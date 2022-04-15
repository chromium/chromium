# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
# limitations under the License..
# ==============================================================================
"""Update version code in the repo.

We use a python script rather than GNU tools to avoid cross-platform
difficulties.

The script takes 3 argument:
 --src <path> a path pointing to the code repo.
 --version <version> the new version code.
 --nightly [default: false] when true, the version code will append a build
   suffix (e.g. dev20201103)

It should not run by bazel. Use it as a simple python script.
"""

import argparse
import datetime
import os
import re

SETUP_PY_PATH = "tensorflow_lite_support/tools/pip_package/setup.py"


def replace_string_in_line(search, replace, filename):
  """Replace the string in every line of the file in-place."""
  with open(filename, "r") as f:
    content = f.read()
  with open(filename, "w") as f:
    f.write(re.sub(search, replace, content))


def get_current_version(path):
  """Get the current version code from setup.py."""
  for line in open(os.path.join(path, SETUP_PY_PATH)):
    match = re.search("^_VERSION = '([a-z0-9\\.\\-]+)'", line)
    if match:
      return match.group(1)
  print("Cannot find current version!")
  return None


def update_version(path, current_version, new_version):
  """Update the version code in the codebase."""
  # Update setup.py
  replace_string_in_line(
      "_VERSION = '%s'" % current_version,
      # pep440 requires such a replacement
      "_VERSION = '%s'" % new_version.replace("-", "."),
      os.path.join(path, SETUP_PY_PATH))


class CustomTimeZone(datetime.tzinfo):

  def utcoffset(self, dt):
    return -datetime.timedelta(hours=8)

  def tzname(self, dt):
    return "UTC-8"

  def dst(self, dt):
    return datetime.timedelta(0)


def remove_build_suffix(version):
  """Remove build suffix (if exists) from a version."""
  if version.find("-dev") >= 0:
    return version[:version.find("-dev")]
  if version.find(".dev") >= 0:
    return version[:version.find(".dev")]
  if version.find("dev") >= 0:
    return version[:version.find("dev")]
  return version


def main():
  parser = argparse.ArgumentParser(description="Update TFLS version in repo")
  parser.add_argument(
      "--src",
      help="a path pointing to the code repo",
      required=True,
      default="")
  parser.add_argument("--version", help="the new SemVer code", default="")
  parser.add_argument(
      "--nightly",
      help="if true, a build suffix will append to the version code. If "
      "current version code or the <version> argument provided contains a "
      "build suffix, the suffix will be replaced with the timestamp",
      action="store_true")
  args = parser.parse_args()

  path = args.src
  current_version = get_current_version(path)
  if not current_version:
    return
  new_version = args.version if args.version else current_version
  if args.nightly:
    new_version = remove_build_suffix(new_version)
    # Use UTC-8 rather than uncertain local time.
    d = datetime.datetime.now(tz=CustomTimeZone())
    new_version += "-dev" + d.strftime("%Y%m%d")
  print("Updating version from %s to %s" % (current_version, new_version))
  update_version(path, current_version, new_version)


if __name__ == "__main__":
  main()
